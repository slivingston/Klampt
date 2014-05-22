# Finds KrisLibrary
# defines the following symbols:
# - KRISLIBRARY_INCLUDE_DIRS
# - KRISLIBRARY_LIBRARIES
# - KRISLIBRARY_DEFINITIONS

IF(NOT KRISLIBRARY_ROOT)
  SET(KRISLIBRARY_ROOT "${CMAKE_SOURCE_DIR}/.."
     CACHE PATH
     "Path for finding KrisLibrary external dependencies"
     FORCE)
ENDIF( )


# Boost threads vs pthreads
SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREADED ON)
SET(Boost_USE_STATIC_RUNTIME OFF)
FIND_PACKAGE(Boost COMPONENTS thread system)
IF(Boost_FOUND)
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DUSE_BOOST_THREADS=1)
  SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
  SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${Boost_LIBRARIES})
ELSE(Boost_FOUND)
  MESSAGE("Boost library not found, trying pthreads for multithreading...")
  FIND_PACKAGE(pthreads)
  IF(pthreads_FOUND) 
    SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -USE_PTHREADS)
    SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} pthreads)
  ENDIF(pthreads_FOUND)
ENDIF(Boost_FOUND)


# GLUI
SET(GLUI_ROOT_DIR "${KRISLIBRARY_ROOT}/glui-2.36/src" CACHE PATH "Root of src directory in GLUI Package" FORCE)
# GLUI
FIND_PACKAGE(GLUI)
IF(GLUI_FOUND)
  MESSAGE("GLUI library found")
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DHAVE_GLUI=1)
  SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${GLUI_INCLUDE_DIR} )
  SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${GLUI_LIBRARY})
ELSE(GLUI_FOUND)
  MESSAGE("GLUI library not found, setting HAVE_GLUI=0")
ENDIF(GLUI_FOUND)

# GLUT
FIND_PACKAGE(GLUT)
IF(GLUT_FOUND)
  MESSAGE("GLUT library found")
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DHAVE_GLUT=1)
  SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${GLUT_INCLUDE_DIR})
  SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${GLUT_LIBRARIES})
ELSE(GLUT_FOUND)
  MESSAGE("GLUT library not found, setting HAVE_GLUT=0")
ENDIF(GLUT_FOUND)

# OpenGL
FIND_PACKAGE(OpenGL REQUIRED)
SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})
SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${OPENGL_LIBRARIES})

# GLPK
SET(GLPK_ROOT "${KRISLIBRARY_ROOT}/glpk-4.52" CACHE PATH "Root of GLPK Package" FORCE)
FIND_PACKAGE(GLPK)
IF(GLPK_FOUND)
  MESSAGE("GLPK library found")
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DHAVE_GLPK=1)
  SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${GLPK_INCLUDE_DIR})
  SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${GLPK_LIBRARY})
ELSE(GLPK_FOUND)
  MESSAGE("GLPK library not found, setting HAVE_GLPK=0")
ENDIF(GLPK_FOUND)

# TinyXML
SET(TINYXML_ROOT "${KRISLIBRARY_ROOT}/tinyxml"
  CACHE PATH
  "Root of TinyXML Package"
  FORCE)
FIND_PACKAGE(TinyXML)
IF(TINYXML_FOUND)
  MESSAGE("TinyXML library found")
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DHAVE_TINYXML=1 -DTIXML_USE_STL)
  SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${TINYXML_INCLUDE_DIR})
  SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${TINYXML_LIBRARY})
ELSE(TINYXML_FOUND)
  MESSAGE("TinyXML library not found, setting HAVE_TINYXML=0")
ENDIF(TINYXML_FOUND)

# Assimp
SET(ASSIMP_ROOT "${KRISLIBRARY_ROOT}/assimp--3.0.1270-sdk" 
   CACHE PATH
   "Root of Assimp package"
   FORCE
)
FIND_PACKAGE(Assimp)
IF(ASSIMP_FOUND)
  MESSAGE("Assimp library found")
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DHAVE_ASSIMP=1)
  SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS} ${ASSIMP_INCLUDE_DIR})
  SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES} ${ASSIMP_LIBRARY})
ELSE(ASSIMP_FOUND)
  MESSAGE("Assimp library not found, setting HAVE_ASSIMP=0")
ENDIF(ASSIMP_FOUND)

IF(WIN32)
  SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS} -DNOMINMAX -DGLUI_NO_LIB_PRAGMA)
ENDIF(WIN32)

SET(KRISLIBRARY_INCLUDE_DIRS ${KRISLIBRARY_INCLUDE_DIRS}
  CACHE STRING
  "KrisLibrary include directories"
  FORCE)
SET(KRISLIBRARY_LIBRARIES ${KRISLIBRARY_LIBRARIES}
  CACHE STRING
  "KrisLibrary link libraries"
  FORCE)
SET(KRISLIBRARY_DEFINITIONS ${KRISLIBRARY_DEFINITIONS}
  CACHE STRING
  "KrisLibrary defines"
  FORCE)