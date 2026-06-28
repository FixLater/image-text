#include "ModuleRegistry.h"

void ModuleRegistry::registerModule(const ModuleInfo &info) {
    modulesInternal().append(info);
}

const QVector<ModuleInfo> &ModuleRegistry::modules() {
    return modulesInternal();
}

QVector<ModuleInfo> &ModuleRegistry::modulesInternal() {
    static QVector<ModuleInfo> s_modules;
    return s_modules;
}
