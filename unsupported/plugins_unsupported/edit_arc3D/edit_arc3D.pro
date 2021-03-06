include (../../shared.pri)

TEMPLATE      = lib

FORMS         = ui/v3dImportDialog.ui
HEADERS       += edit_arc3D.h   \
				edit_arc3D_factory.h   \
				arc3D_reconstruction.h \
				arc3D_camera.h \
				radial_distortion.h\
				v3dImportDialog.h \
				scalar_image.h \
								maskRenderWidget.h \
								maskImageWidget.h \
								fillImage.h

SOURCES       += edit_arc3D.cpp   \
        edit_arc3D_factory.cpp   \
                arc3D_camera.cpp \
                radial_distortion.cpp \
                scalar_image.cpp \
                v3dImportDialog.cpp \
                maskRenderWidget.cpp \
                maskImageWidget.cpp \
                fillImage.cpp \
    $$VCGDIR/wrap/ply/plylib.cpp

TARGET        = edit_arc3D
RESOURCES     = edit_arc3D.qrc

win32-msvc2005:LIBS	   += ../../external/lib/win32-msvc2005/bz2.lib
win32-msvc2008:LIBS	   += ../../external/lib/win32-msvc2008/bz2.lib
win32-msvc2010:LIBS	   += ../../external/lib/win32-msvc2010/bz2.lib
win32-msvc2012:LIBS	   += ../../external/lib/win32-msvc2012/bz2.lib
win32-msvc2013:LIBS	   += ../../external/lib/win32-msvc2013/bz2.lib
win32-msvc2015:LIBS	   += ../../external/lib/win32-msvc2015/bz2.lib
win32-g++:LIBS	+= ../../external/lib/win32-gcc/libbz2.a
linux-g++:LIBS	+= -lbz2
mac:LIBS   += -lbz2

!CONFIG(system_bzip2):INCLUDEPATH += ../../external/bzip2-1.0.5


CONFIG(release, debug|release) {
	win32-g++:release:QMAKE_CXXFLAGS -= -O2
	win32-g++:release:QMAKE_CFLAGS -= -O2
	win32-g++:release:QMAKE_CXXFLAGS += -O3 -mtune=pentium3 -ffast-math
}

