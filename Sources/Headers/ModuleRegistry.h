#ifndef MODULEREGISTRY_H
#define MODULEREGISTRY_H

#include <QVector>
#include "ModuleInfo.h"

class ModuleRegistry {
public:
    static void registerModule(const ModuleInfo &info);
    static const QVector<ModuleInfo> &modules();

private:
    static QVector<ModuleInfo> &modulesInternal();
};

#endif // MODULEREGISTRY_H
