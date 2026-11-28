plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val releaseStoreFile = System.getenv("NEXA_ANDROID_KEYSTORE_PATH")
val releaseStorePassword = System.getenv("NEXA_ANDROID_STORE_PASSWORD")
val releaseKeyAlias = System.getenv("NEXA_ANDROID_KEY_ALIAS")
val releaseKeyPassword = System.getenv("NEXA_ANDROID_KEY_PASSWORD")
val nexaVersionName = providers.gradleProperty("nexaVersionName").orElse("2.34.0").get()
val nexaVersionCode = providers.gradleProperty("nexaVersionCode").orElse("23400").get().toInt()

android {
    signingConfigs {
        if (!releaseStoreFile.isNullOrBlank() && !releaseStorePassword.isNullOrBlank() &&
            !releaseKeyAlias.isNullOrBlank() && !releaseKeyPassword.isNullOrBlank()) {
            create("release") {
                storeFile = file(releaseStoreFile)
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
            }
        }
    }
    namespace = "com.nexa.machine"
    compileSdk = 35
    defaultConfig {
        applicationId = "com.nexa.machine"
        minSdk = 26
        targetSdk = 35
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndkVersion = "27.2.12479018"
        versionCode = nexaVersionCode
        versionName = nexaVersionName
        externalNativeBuild {
            cmake { cppFlags += "-std=c++20"; arguments += listOf("-DNEXA_BUILD_HOST_TOOLS=OFF", "-DNEXA_BUILD_GUI=OFF") }
        }
    }
    buildTypes {
        getByName("release") {
            isMinifyEnabled = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (signingConfigs.findByName("release") != null) signingConfig = signingConfigs.getByName("release")
        }
    }
    buildFeatures { compose = true }
    composeOptions { kotlinCompilerExtensionVersion = "1.5.15" }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
    externalNativeBuild { cmake { path = file("../../../CMakeLists.txt"); version = "3.22.1" } }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.12.01"))
    implementation("androidx.activity:activity-compose:1.10.0")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    debugImplementation("androidx.compose.ui:ui-tooling")
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test:rules:1.6.1")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
}
