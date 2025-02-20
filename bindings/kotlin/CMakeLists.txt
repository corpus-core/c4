# SWIG muss installiert und in deinem PATH sein.
find_package(SWIG REQUIRED)
include(UseSWIG)

# Wir verwenden reines C (kein C++), deshalb schalten wir C++ aus.
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/proofer.i PROPERTIES CPLUSPLUS OFF)

# Setze SWIG-Flags: -module gibt den Modulnamen an.
set(CMAKE_SWIG_FLAGS "-module" "c4")

message(STATUS "JAVA_HOME: $ENV{JAVA_HOME}")
# Optional: Falls du Java-Unterstützung für SWIG benötigst:
find_package(Java REQUIRED)
include(UseJava)
if(NOT Java_INCLUDE_DIRS)
    if(APPLE)
        # macOS: JNI-Header liegen unter include und include/darwin
        set(Java_INCLUDE_DIRS "$ENV{JAVA_HOME}/include" "$ENV{JAVA_HOME}/include/darwin")
    elseif(UNIX)
        # Linux: JNI-Header liegen unter include und include/linux
        set(Java_INCLUDE_DIRS "$ENV{JAVA_HOME}/include" "$ENV{JAVA_HOME}/include/linux")
    elseif(WIN32)
        # Windows: typischerweise include und include/win32
        set(Java_INCLUDE_DIRS "$ENV{JAVA_HOME}/include" "$ENV{JAVA_HOME}/include/win32")
    endif()
    message(WARNING "Java include directories manually set to: ${Java_INCLUDE_DIRS}")
endif()
include_directories(${Java_INCLUDE_DIRS})
message(STATUS "Java include directories: ${Java_INCLUDE_DIRS}")

# Stelle sicher, dass der Pfad zu deinen Headern (und dem SWIG-Interface) stimmt.
include_directories(${CMAKE_SOURCE_DIR}/bindings)

set(CMAKE_SWIG_FLAGS -package;com.corpuscore.colibri;-outdir;${CMAKE_CURRENT_BINARY_DIR}/java/com/corpuscore/colibri;-I${CMAKE_SOURCE_DIR}/bindings)
swig_add_library(c4_java
    LANGUAGE java
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/java/com/corpuscore/colibri
    OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/java    
    SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/colibri.i
       ${CMAKE_CURRENT_SOURCE_DIR}/../colibri.c
)
# Beispiel: Für Android solltest du oft auch die log-Bibliothek verlinken.
#find_library(log-lib log)
target_link_libraries(c4_java proofer util verifier)


# Compile the generated Java files
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/classes/com/corpuscore/colibri/c4.class
    COMMAND ${Java_JAVAC_EXECUTABLE} -d ${CMAKE_CURRENT_BINARY_DIR}/classes ${CMAKE_CURRENT_BINARY_DIR}/java/com/corpuscore/colibri/*.java
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/java/com/corpuscore/colibri/c4.java
    COMMENT "Compiling generated Java files"
)


# Add a custom target to build the Java classes
add_custom_target(CompileJava ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/classes/com/corpuscore/colibri/c4.class
)


# Compile Kotlin files
set(KOTLIN_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/lib/src/main/kotlin/com/corpuscore/colibri/Colibri.kt")
set(KOTLIN_OUTPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/lib/build/classes/kotlin/main/com/corpuscore/colibri/Colibri.class")

# Compile Kotlin files using Gradle
add_custom_command(
    OUTPUT ${KOTLIN_OUTPUT_FILE}
    COMMAND ./gradlew build -PcustomBuildDir=${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${KOTLIN_SOURCE_FILE}
    COMMENT "Building Kotlin files with Gradle"
)


add_custom_target(CompileKotlin ALL
    DEPENDS ${KOTLIN_OUTPUT_FILE}
)


# Make CompileKotlin dependent on CompileJava
add_dependencies(CompileKotlin CompileJava)