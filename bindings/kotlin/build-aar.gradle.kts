plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
}

group = "com.corpuscore"
version = "1.0.0" // Adjust as needed

android {
    compileSdk = 33
    defaultConfig {
        minSdk = 21
        targetSdk = 33
    }
    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            // Pass argument to skip Java source generation, as they are pre-generated
            arguments("-DGENERATE_JAVA_SOURCES=OFF", "-DKOTLIN=true", "-DCURL=false")
            // Specify ABIs to build for
            abiFilters("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }
    }
    ndkVersion = "23.1.7779620" // Adjust to your NDK version
    sourceSets {
        getByName("main") {
            java.srcDirs(file("${projectDir}/generated/java"))
        }
    }
}

dependencies {

    api(libs.commons.math3)
    implementation(libs.guava)
    implementation("io.ktor:ktor-client-core:2.0.0")
    implementation("io.ktor:ktor-client-cio:2.0.0") // CIO engine for asynchronous requests
    implementation("io.ktor:ktor-client-json:2.0.0")
    implementation("io.ktor:ktor-client-serialization:2.0.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.6.0")
    implementation("org.json:json:20210307")
    implementation("org.jetbrains.kotlin:kotlin-stdlib")

}

publishing {
    publications {
        create<MavenPublication>("aar") {
            from(components["release"])
            artifactId = "colibri-aar"
        }
    }
    repositories {
        maven {
            url = uri("https://your.maven.repo") // Replace with your Maven repository URL
            credentials {
                username = project.findProperty("mavenUsername")?.toString()
                password = project.findProperty("mavenPassword")?.toString()
            }
        }
    }
}