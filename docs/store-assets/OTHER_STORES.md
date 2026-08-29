# Amazon Appstore & Samsung Galaxy Store

Both accept the **exact same signed APK** CI already builds for every release —
no separate build, flavor, or code change. Account creation and the actual
submission forms are things only you can do; this page just says what to
upload and what to expect.

## Where the APK comes from

Every tagged release (`.github/workflows/build.yml`) uploads it as the
`Android` artifact (`frozen-bubble-android-tv.apk`), signed with the same
persistent release key Play Store gets (see
[ANDROID_SIGNING.md](../ANDROID_SIGNING.md)). Grab it from the release's
GitHub Actions run, or from the release page itself once tagged. This is a
plain installable APK — the same one itch.io hosts for direct download —
*not* the `.aab` (that format is Play-only).

## Amazon Appstore

Submit the APK as-is through the
[Amazon Developer Console](https://developer.amazon.com/apps-and-games).

**Known limitation, accepted as-is:** Amazon Fire OS devices ship without
Google Play Services — no Play Store app, no Play Billing, and AdMob's ad
SDK is unreliable without it. Concretely, on Fire OS:
- Ads (`AdsManager.java`, AdMob) may not load/show.
- The "Remove Ads" in-app purchase (Google Play Billing) will not work at
  all — Play Billing requires the Play Store app to process a purchase.

Decided (2026-08-28) to ship the identical build anyway rather than build an
Amazon-specific flavor with Amazon's own IAP/ad SDKs — revisit only if this
turns out to matter (e.g. real Fire OS install numbers, or user reports of a
broken purchase button). A proper fix would mean a separate Amazon-flavored
build swapping in Amazon's IAP SDK and ad network, which is real engineering
work, not a config toggle.

Devices that DO have Google Play Services installed alongside Fire OS's own
store (rare, but some Amazon tablets/phones do) work identically to Play —
this limitation is specifically about the no-Play-Services Fire OS case.

## Samsung Galaxy Store

Submit the same APK through the
[Samsung Seller Portal](https://seller.samsungapps.com/). No known
limitation here — the overwhelming majority of Samsung phones ship with
both Google Play Services and Play Store pre-installed alongside Galaxy
Store (Galaxy Store is additive, not a Play replacement), so ads and Play
Billing purchases work the same as a Play Store install.

## Listing content

Reuse the store listing text, screenshots, and content-rating answers
already drafted in [PLAY_LISTING.md](PLAY_LISTING.md) — none of it is
Play-specific. Screenshots in this directory (`screenshot-*.png`,
`icon-512.png`, `feature-graphic.png`) work for any store's asset
requirements; check each console's own size/format rules before upload.
