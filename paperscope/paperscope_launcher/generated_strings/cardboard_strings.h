// GENERATED CODE : DO NOT EDIT BY HAND
#ifndef VR_APPS_CARDBOARD_DEMO_COMMON_TOOLS_CARDBOARD_STRINGS_H_
#define VR_APPS_CARDBOARD_DEMO_COMMON_TOOLS_CARDBOARD_STRINGS_H_

#include <string>
#include <jni.h>

namespace cardboard {

typedef enum {
  kStrTutorialDemoTitle,
  kStrWelcomeVideoTitle,
  kStrExplorerDemoTitle,
  kStrExhibitDemoTitle,
  kStrUrbanHikeDemoTitle,
  kStrKaleidoscopeDemoTitle,
  kStrArcticJourneyDemoTitle,
  kStrConnectionErrorMessage,
  kStrGenericErrorMessage,
  kStrTutorialClickCaption,
  kStrTutorialTiltCaption,
  kStrHawkRattle,
  kStrVaticanMask,
  kStrChiefsMask,
  kStrTheVisionary,
  kStrBeethoven,
  kStrAmericanMuseumNaturalHistory,
  kStrFrontiersOfFlight,
  kStrEndeavourSpaceShuttle,
  kStrCuevaDelIndio,
  kStrGunnuhverHotSprings,
  kStrMarsSpiritRover,
  kStrUrbanHikeCurrentLocation,
  kStrUrbanHikeParis,
  kStrUrbanHikeTokyo,
  kStrUrbanHikeVenice,
  kStrUrbanHikeNewYork,
  kStrUrbanHikeRome,
  kStrUrbanHikeJerusalem,
  kStrUrbanHikeMonteCarlo,
  kStrUrbanHikeLondon,
  kStrUrbanHikeGreatBarrierReef,
  kStrEarthAndroidDemoTitle,
  kStrTourGuideAndroidDemoTitle,
  kStrMyVideosAndroidDemoTitle,
  kStrPhotoSphereAndroidDemoTitle,
  kStrWindyDayAndroidDemoTitle,
  kStrExit,
  kStrBackToDemos,
  kStrRotateToGoBack,
  kStrTakeYourPhoneOutToExitDemos,
  kStrNext,
  kStrPressTheButtonOnYourViewer,
  kStrRotateYourViewer,
  kStrTryItOutNow,
} StringId;

const char* GetLocalizedString(StringId string_id, JNIEnv* env);
const char* GetLocalizedString(const std::string& key, JNIEnv* env);
const char* GetLocalizedStringForLocale(StringId string_id,
                                        const std::string& locale,
                                        JNIEnv* env);
const char* GetLocalizedStringForLocale(const std::string& key,
                                        const std::string& locale,
                                        JNIEnv* env);

}  // namespace cardboard

#endif  // VR_APPS_CARDBOARD_DEMO_COMMON_TOOLS_CARDBOARD_STRINGS_H_
