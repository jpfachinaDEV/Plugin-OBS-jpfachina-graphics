#pragma once
#include "store.hpp"
#include <obs.h>
#include <QImage>
#include <functional>
#include <QJsonObject>
#include <atomic>
namespace jp {
constexpr const char *nativeSourceId="jp_fachina_composition";
void registerNativeSource();
QJsonObject nativeState(Store &store,const QJsonObject &project,const QString &selected={},const QJsonObject &draft={});
void updateNativeSource(obs_source_t *source,const QJsonObject &state);
QImage nativeThumbnail(obs_source_t *preview);
void nativeThumbnailAsync(obs_source_t *preview,QObject *context,std::function<void(QImage)> completed);
QJsonObject nativeObserved(obs_source_t *source);
}
