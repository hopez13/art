#!/bin/bash -ex
m public.libraries-oem1.txt \
  public.libraries-oem2.txt \
  public.libraries-product1.txt \
  oemlibrarytest-system \
  oemlibrarytest-vendor
adb root
adb wait-for-device
adb remount
# If the above says "please reboot" then we need to do that, and remount again.
adb sync
adb shell stop
adb shell start
adb wait-for-device
adb logcat -c;
adb shell am start -n android.test.app.system/android.test.app.TestActivity
adb shell am start -n android.test.app.vendor/android.test.app.TestActivity
adb logcat | grep android.test.app
