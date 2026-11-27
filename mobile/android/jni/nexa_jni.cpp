#include <jni.h>
#include "nexa/c_api.h"
#include <cstdint>
#include <string>

extern "C" JNIEXPORT jlong JNICALL
Java_com_nexa_machine_NexaNative_createContext(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(nexa_context_create());
}
extern "C" JNIEXPORT void JNICALL
Java_com_nexa_machine_NexaNative_destroyContext(JNIEnv*, jclass, jlong handle) {
    nexa_context_destroy(reinterpret_cast<NexaContext*>(handle));
}
extern "C" JNIEXPORT jlong JNICALL
Java_com_nexa_machine_NexaNative_createSession(JNIEnv* env, jclass, jlong handle, jstring name, jint ramBytes) {
    auto* context = reinterpret_cast<NexaContext*>(handle);
    if (!context || !name || ramBytes <= 0) return 0;
    const char* text = env->GetStringUTFChars(name, nullptr);
    if (!text) return 0;
    uint64_t id = 0;
    const int rc = nexa_session_create(context, text, static_cast<size_t>(ramBytes), &id);
    env->ReleaseStringUTFChars(name, text);
    return rc == 0 ? static_cast<jlong>(id) : 0;
}
extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_loadAssembly(JNIEnv* env, jclass, jlong handle, jlong session, jstring source) {
    auto* context = reinterpret_cast<NexaContext*>(handle);
    if (!context || !source) return -1;
    const char* text = env->GetStringUTFChars(source, nullptr);
    if (!text) return -1;
    const int rc = nexa_session_load_assembly(context, static_cast<uint64_t>(session), text);
    env->ReleaseStringUTFChars(source, text);
    return rc;
}
extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_run(JNIEnv*, jclass, jlong handle, jlong session, jint budget) {
    if (budget <= 0) return -1;
    NexaSessionState state = NEXA_READY;
    return nexa_session_run(reinterpret_cast<NexaContext*>(handle), static_cast<uint64_t>(session), static_cast<size_t>(budget), &state);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_step(JNIEnv*, jclass, jlong handle, jlong session) {
    int alive = 0;
    return nexa_session_step(reinterpret_cast<NexaContext*>(handle), static_cast<uint64_t>(session), &alive);
}
extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_pause(JNIEnv*, jclass, jlong handle, jlong session) {
    return nexa_session_pause(reinterpret_cast<NexaContext*>(handle), static_cast<uint64_t>(session));
}
extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_reset(JNIEnv*, jclass, jlong handle, jlong session) {
    return nexa_session_reset(reinterpret_cast<NexaContext*>(handle), static_cast<uint64_t>(session));
}
extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_sendInput(JNIEnv* env, jclass, jlong handle, jlong session, jstring input) {
    auto* context = reinterpret_cast<NexaContext*>(handle);
    if (!context || !input) return -1;
    const char* text = env->GetStringUTFChars(input, nullptr);
    if (!text) return -1;
    const int rc = nexa_keyboard_input(context, static_cast<uint64_t>(session), text);
    env->ReleaseStringUTFChars(input, text);
    return rc;
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_nexa_machine_NexaNative_lastError(JNIEnv* env, jclass, jlong handle) {
    const char* error = nexa_last_error(reinterpret_cast<NexaContext*>(handle));
    return env->NewStringUTF(error ? error : "Unknown Nexa error");
}

extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_stop(JNIEnv*, jclass, jlong handle, jlong session) {
    return nexa_session_stop(reinterpret_cast<NexaContext*>(handle), static_cast<uint64_t>(session));
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_nexa_machine_NexaNative_console(JNIEnv* env, jclass, jlong handle, jlong session) {
    auto* context = reinterpret_cast<NexaContext*>(handle);
    if (!context) return env->NewStringUTF("");
    size_t required = 0;
    if (nexa_console_read(context, static_cast<uint64_t>(session), nullptr, 0, &required) != 0 || required == 0)
        return env->NewStringUTF("");
    std::string output(required, '\0');
    if (nexa_console_read(context, static_cast<uint64_t>(session), output.data(), output.size(), &required) != 0)
        return env->NewStringUTF("");
    return env->NewStringUTF(output.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_nexa_machine_NexaNative_state(JNIEnv*, jclass, jlong handle, jlong session) {
    NexaSessionInfo info{};
    const int rc = nexa_session_info(reinterpret_cast<NexaContext*>(handle), static_cast<uint64_t>(session), &info);
    return rc == 0 ? static_cast<jint>(info.state) : -1;
}
