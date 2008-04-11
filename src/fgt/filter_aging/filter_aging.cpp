/****************************************************************************
* MeshLab                                                           o o     *
* A versatile mesh processing toolbox                             o     o   *
*                                                                _   O  _   *
* Copyright(C) 2005                                                \/)\/    *
* Visual Computing Lab                                            /\/|      *
* ISTI - Italian National Research Council                           |      *
*                                                                    \      *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *   
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
*                                                                           *
* This program is distributed in the hope that it will be useful,           *
* but WITHOUT ANY WARRANTY; without even the implied warranty of            *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
* GNU General Public License (http://www.gnu.org/licenses/gpl.txt)          *
* for more details.                                                         *
*                                                                           *
****************************************************************************/


#include <map>
#include <vector>
//#include <iostream>		// DEBUG

#include "filter_aging.h"

#include <vcg/complex/trimesh/update/position.h>
#include <vcg/complex/trimesh/update/bounding.h>
#include <vcg/complex/trimesh/stat.h>
#include <vcg/math/perlin_noise.h>
#include <vcg/complex/trimesh/clean.h>
#include <vcg/complex/trimesh/closest.h>
#include <vcg/space/index/grid_static_ptr.h>


// Constructor 
GeometryAgingPlugin::GeometryAgingPlugin() 
{ 
	typeList << FP_ERODE;
	
	foreach(FilterIDType tt, types())
		actionList << new QAction(filterName(tt), this);
}


// Destructor 
GeometryAgingPlugin::~GeometryAgingPlugin() 
{ 
}


// Returns the very short string describing each filtering action
const QString GeometryAgingPlugin::filterName(FilterIDType filterId) 
{
	switch(filterId) {
		case FP_ERODE:
			return QString("Aging Simulation");
		default:
			assert(0);
	}
}


// Returns the longer string describing each filtering action
const QString GeometryAgingPlugin::filterInfo(FilterIDType filterId) 
{
	switch(filterId) {
		case FP_ERODE: 
			return QString("Simulates the aging effects due to small collisions or various chipping events");
		default: 
			assert(0);
	}
}


// Returns plugin info
const PluginInfo &GeometryAgingPlugin::pluginInfo() 
{
	static PluginInfo ai;
	ai.Date = tr(__DATE__);
	ai.Version = tr("0.1");
	ai.Author = tr("Marco Fais");
	return ai;
 }


// Initializes the list of parameters (called by the auto dialog framework)
void GeometryAgingPlugin::initParameterSet(QAction *action, MeshModel &m, FilterParameterSet &params)
{
	std::pair<float,float> qRange;
	if(m.cm.HasPerVertexQuality())
		qRange = tri::Stat<CMeshO>::ComputePerVertexQualityMinMax(m.cm);
	else
		qRange = std::pair<float,float>(0.0, 255.0);
	QStringList styles;
	// TODO: find better strings to list styles 
	styles.append("Simple     (1 function) ");
	styles.append("Linear     (8 functions)");
	styles.append("Sinusoidal (8 functions)");
	
	switch(ID(action)) {
		case FP_ERODE:
			params.addBool("UseQuality", m.cm.HasPerVertexQuality(), "Use per vertex quality values", 
					"Use per vertex quality values to find the areas to erode.\n Useful after applying ambient occlusion filter.");
			params.addAbsPerc("QualityThreshold", qRange.first+(qRange.second-qRange.first)*2/3, qRange.first, qRange.second,  "Quality threshold", 
					"When you decide to use per vertex quality values, \n this parameter represents the minimum quality value \n \
					two vertexes must have to consider the edge they are sharing.");
			params.addAbsPerc("AngleThreshold", 60.0, 10.0, 170.0,  "Angle threshold (deg)", 
					"If you decide not to use per vertex quality values, \nthe angle between two adjacent faces will be considered.\n \
					This parameter represents the minimum angle between two adjacent\n faces to consider the edge they are sharing.");
			params.addAbsPerc("EdgeLenThreshold", m.cm.bbox.Diag()*0.02, 0.0, m.cm.bbox.Diag()*0.5,"Edge len threshold", 
					"The minimum length of an edge. Useful to avoid the creation of too many small faces.");
			params.addAbsPerc("ChipDepth", m.cm.bbox.Diag()*0.004, 0.0, m.cm.bbox.Diag()*0.1,"Max chip depth", 
					"The maximum depth of a chip.");
			params.addEnum("ChipStyle", 0, styles, "Chip Style", "Mesh erosion style to use. Different styles are defined \
					passing different parameters to the noise function \nthat generates displacement values.");
			params.addBool("Selected", m.cm.sfn>0, "Affect only selected faces", 
					"The aging procedure will be applied to the selected faces only.");
			break;
		default:
			assert(0);
	}
}


// The Real Core Function doing the actual mesh processing
bool GeometryAgingPlugin::applyFilter(QAction *filter, MeshModel &m, FilterParameterSet &params, vcg::CallBackPos *cb)
{
	bool useQuality = params.getBool("UseQuality");
	if(useQuality && !m.cm.HasPerVertexQuality()) {
		errorMessage = QString("This mesh doesn't have per vertex quality informations.");
		return false;
	}
	
	float qualityThreshold = params.getAbsPerc("QualityThreshold");
	float angleThreshold = params.getAbsPerc("AngleThreshold");
	float edgeLenTreshold = params.getAbsPerc("EdgeLenThreshold");
	float chipDepth = params.getAbsPerc("ChipDepth");
	int chipStyle = params.getEnum("ChipStyle");
	bool selected = params.getBool("Selected");
	
	AgingEdgePred ep;					// edge predicate
	CMeshO::FaceIterator fi, endfi;
	CMeshO::VertexIterator vi;
	int fcount = 1;					// face counter (to update progress bar)
	
	//typedef std::pair<int, Point3<CVertexO::ScalarType> > vertOff;
	typedef std::map<CVertexO*, Point3<CVertexO::ScalarType> > DispMap;
	DispMap displaced;					// displaced vertexes and their displacement offset 
	std::vector<CFaceO*> intersFace;	// self intersecting mesh faces (after displacement)
	
	if(useQuality)
		ep = AgingEdgePred(AgingEdgePred::QUALITY, edgeLenTreshold, qualityThreshold);
	else
		ep = AgingEdgePred(AgingEdgePred::ANGLE, edgeLenTreshold, angleThreshold);
	
	srand(time(NULL));
	
	switch(ID(filter)) {
		case FP_ERODE:
			// refine needed edges
			while(RefineE<CMeshO, MidPoint<CMeshO>, AgingEdgePred>(m.cm, MidPoint<CMeshO>(), ep, selected, cb))
				vcg::tri::UpdateNormals<CMeshO>::PerVertexNormalizedPerFace(m.cm);
			
			// displace vertexes
			for(fi = m.cm.face.begin(), endfi=m.cm.face.end(); fi != endfi; fi++) {
				if(cb) (*cb)((fcount++/m.cm.fn)*100, "Aging..."); 
				if((*fi).IsD()) continue;
				for(int j=0; j<3; j++) {
					if(ep.qaVertTest(face::Pos<CMeshO::FaceType>(&*fi,j))  && 
					  (!selected || ((*fi).IsS() && (*fi).FFp(j)->IsS())) ) { 
						double noise;							// noise value
						Point3<CVertexO::ScalarType> dispDir;	// displacement direction
						float angleFactor;
						
						noise = generateNoiseValue(chipStyle, (*fi).V(j)->P().X(), (*fi).V(j)->P().Y(), (*fi).V(j)->P().Z());
						noise *= (noise>=0.0?1.0:-1.0);
						
						if((*fi).IsB(j)) {
							dispDir = Point3<CVertexO::ScalarType>((*fi).N().Normalize());
							angleFactor = 1.0;
						}
						else { 
							if(useQuality)
								dispDir = Point3<CVertexO::ScalarType>(((*fi).N() + (*fi).FFp(j)->N()).Normalize());
							else
								dispDir = Point3<CVertexO::ScalarType>((*fi).N().Normalize());
							angleFactor = (sin(vcg::Angle((*fi).N(), (*fi).FFp(j)->N()))/2 + 0.5);
						}
						
						// displacement offset
						Point3<CVertexO::ScalarType> offset = -(dispDir * chipDepth * noise * angleFactor);
						if(displaced.find((*fi).V(j)) == displaced.end())
							displaced.insert(std::pair<CVertexO*, Point3<CVertexO::ScalarType> >((*fi).V(j), (*fi).V(j)->P()));
						(*fi).V(j)->P() += offset;
					}
				}
			}
			
			// avoid mesh self intersection
			if(!displaced.empty()) {
				DispMap::iterator it;
				int intnum;
				fcount = 0;
				// user parameter instead 20 as max iteration limit?
				for(int t=0; t<20; t++) {
					intersFace.clear();
					std::vector<CFaceO *>::iterator fpi;
					tri::Clean<CMeshO>::SelfIntersections(m.cm, intersFace);
					if(t == 0) 
						intnum = intersFace.size();
					else
						fcount += intnum - intersFace.size(); 
					if(intersFace.empty()) break;
					if(cb) (*cb)((fcount/intnum)*100, "Deleting mesh intersections...");
					for(fpi=intersFace.begin(); fpi!=intersFace.end(); fpi++) {
						for(int i=0; i<3; i++) {
							it = displaced.find((*fpi)->V(i));
							if(it != displaced.end())
								(*fpi)->V(i)->P() = ((*fpi)->V(i)->P()+(*it).second)/2.0;
						}
						if(displaced.empty()) break;
					}
				}
			}
			
			vcg::tri::UpdateNormals<CMeshO>::PerVertexNormalizedPerFace(m.cm);
			
			return true;
		default:
			assert(0);
	}
}


// returns a noise value
double GeometryAgingPlugin::generateNoiseValue(int style, const CVertexO::ScalarType &x, 
											const CVertexO::ScalarType &y, const CVertexO::ScalarType &z)
{
	double noise = .0;
	
	switch(style) {
		case SIMPLE:
			noise = math::Perlin::Noise(x, y, z);
			break;
		case LINEAR:
		case SINUSOIDAL:
			for(int i=0; i<8; i++) {
				float p = pow(2.0f, i);
				noise += math::Perlin::Noise(p*x, p*y, p*z) / p;
			}
			noise /= 2.0;
			noise *= (noise>=0.0?1.0:-1.0);
			break;
	}

	if(style == 2) {
		noise /= 4.0;
		noise = sin(x + noise);
	}
	
	return noise;
}


Q_EXPORT_PLUGIN(GeometryAgingPlugin)
