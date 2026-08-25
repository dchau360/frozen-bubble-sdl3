/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */
package org.frozenbubble;

import android.app.Activity;
import android.util.Log;

import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import androidx.annotation.NonNull;

/**
 * Handles Google Play Billing for the two "Remove Ads" products.
 *
 * Product IDs to create in Google Play Console:
 *   "remove_ads_year"    -- SUBSCRIPTION, ~$5/year, auto-renewing
 *   "remove_ads_forever" -- ONE-TIME purchase, ~$15
 *
 * The yearly one is a real subscription rather than a self-expiring one-time
 * purchase on purpose. Without a backend, a "1 year pass" sold as a one-time
 * product can only track its own expiry from Purchase.getPurchaseTime() -- and
 * letting somebody buy a second year means consuming the first, which throws
 * that timestamp away and takes the entitlement with it on the next reinstall.
 * Play tracks subscription state itself, so queryPurchasesAsync(SUBS) answers
 * "is it active right now" correctly across reinstalls and new devices with no
 * server of ours involved.
 *
 * Usage:
 *   BillingManager mgr = new BillingManager(activity);
 *   mgr.launchPurchaseFlow(BillingManager.PRODUCT_YEAR);
 *   mgr.destroy();             // call in onDestroy()
 */
public class BillingManager implements PurchasesUpdatedListener {
    private static final String TAG = "FBubble.Billing";

    /** Auto-renewing yearly subscription. */
    public  static final String PRODUCT_YEAR    = "remove_ads_year";
    /** One-time permanent purchase. */
    public  static final String PRODUCT_FOREVER = "remove_ads_forever";

    private final Activity      mActivity;
    private       BillingClient mBillingClient;
    private final Map<String, ProductDetails> mProductDetails = new HashMap<>();

    // The renderer needs localized prices for the two Settings rows, and it
    // reaches Java through a static JNI call with no handle to the Activity's
    // BillingManager instance. One Activity means one instance, so a static
    // pointer to it is enough; null simply means "not constructed yet", which
    // getPrice() reports as an empty price.
    private static volatile BillingManager sInstance = null;

    public BillingManager(Activity activity) {
        mActivity = activity;
        mBillingClient = BillingClient.newBuilder(activity)
                .setListener(this)
                .enablePendingPurchases()
                .build();
        sInstance = this;
        connectAndRestore();
    }

    /**
     * Start the Play purchase UI for one product id (PRODUCT_YEAR or
     * PRODUCT_FOREVER).
     */
    public void launchPurchaseFlow(String productId) {
        ProductDetails details = mProductDetails.get(productId);
        if (details == null) {
            Log.w(TAG, "Product details not loaded for " + productId + " — retrying connect");
            connectAndRestore();
            return;
        }

        BillingFlowParams.ProductDetailsParams.Builder pdp =
                BillingFlowParams.ProductDetailsParams.newBuilder()
                        .setProductDetails(details);

        // A subscription purchase must name which base plan / offer is being
        // bought; a one-time product has no such token and setting one is an
        // error. This is the only structural difference between the two flows.
        if (PRODUCT_YEAR.equals(productId)) {
            List<ProductDetails.SubscriptionOfferDetails> offers =
                    details.getSubscriptionOfferDetails();
            if (offers == null || offers.isEmpty()) {
                Log.w(TAG, "Subscription " + productId + " has no offers configured");
                return;
            }
            pdp.setOfferToken(offers.get(0).getOfferToken());
        }

        BillingFlowParams params = BillingFlowParams.newBuilder()
                .setProductDetailsParamsList(Collections.singletonList(pdp.build()))
                .build();
        mBillingClient.launchBillingFlow(mActivity, params);
    }

    /** Call on Activity.onDestroy() */
    public void destroy() {
        if (sInstance == this) sInstance = null;
        if (mBillingClient.isReady()) {
            mBillingClient.endConnection();
        }
    }

    // --- PurchasesUpdatedListener ---

    @Override
    public void onPurchasesUpdated(@NonNull BillingResult result,
                                   List<Purchase> purchases) {
        if (result.getResponseCode() == BillingClient.BillingResponseCode.OK
                && purchases != null) {
            for (Purchase p : purchases) {
                handlePurchase(p);
                // Grant straight away rather than waiting for the next
                // restorePurchases(): the player just paid and expects the ads
                // to stop now, not on the next launch. Only ever set true here
                // -- lapsing is decided by restorePurchases(), which sees the
                // whole picture.
                if (isActive(p, PRODUCT_FOREVER) || isActive(p, PRODUCT_YEAR)) {
                    applyEntitlement(true);
                }
            }
        } else if (result.getResponseCode() == BillingClient.BillingResponseCode.USER_CANCELED) {
            Log.d(TAG, "User cancelled purchase");
        } else if (result.getResponseCode()
                       == BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED) {
            // Buying something already owned is not an error worth showing --
            // it just means our local flag was behind. Re-derive it.
            Log.d(TAG, "Item already owned — restoring entitlement");
            restorePurchases();
        } else {
            Log.w(TAG, "Purchase error: " + result.getDebugMessage());
        }
    }

    // --- private helpers ---

    private void connectAndRestore() {
        mBillingClient.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(@NonNull BillingResult result) {
                if (result.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                    Log.d(TAG, "Billing connected");
                    restorePurchases();
                    queryProductDetails();
                }
            }
            @Override
            public void onBillingServiceDisconnected() {
                Log.w(TAG, "Billing disconnected");
            }
        });
    }

    /**
     * Re-derives the ads-removed entitlement from Play, for both product types.
     *
     * This *recomputes* rather than only granting, because the yearly plan can
     * lapse: a subscription that expired or was cancelled stops appearing in
     * queryPurchasesAsync(SUBS), and if that only ever set the flag true, ads
     * would stay off forever after a single expired year. Both queries are
     * asynchronous and independent, so the results are combined once the second
     * one lands rather than each writing the flag on its own.
     *
     * A query that comes back non-OK (a disconnect, a transient Play error)
     * still has to mark itself finished, or the other query's result is never
     * applied at all -- a paying subscriber would keep seeing ads for the whole
     * session because an unrelated INAPP lookup happened to fail. But a failed
     * query is not evidence of *not* owning anything either, so revoking is
     * held back unless both queries actually answered. Grant on any evidence,
     * revoke only on complete evidence.
     *
     * The unsynchronized arrays are safe because Play delivers both callbacks
     * on the main thread; they are not a general-purpose concurrent combine.
     */
    private void restorePurchases() {
        final boolean[] owned = new boolean[2];
        final boolean[] done  = new boolean[2];
        final boolean[] ok    = new boolean[2];

        mBillingClient.queryPurchasesAsync(
                QueryPurchasesParams.newBuilder()
                        .setProductType(BillingClient.ProductType.INAPP)
                        .build(),
                (billingResult, purchases) -> {
                    if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        for (Purchase p : purchases) {
                            handlePurchase(p);
                            if (isActive(p, PRODUCT_FOREVER)) owned[0] = true;
                        }
                        ok[0] = true;
                    } else {
                        Log.w(TAG, "INAPP purchase query failed: "
                                + billingResult.getDebugMessage());
                    }
                    done[0] = true;
                    if (done[1]) combineRestore(owned, ok);
                });

        mBillingClient.queryPurchasesAsync(
                QueryPurchasesParams.newBuilder()
                        .setProductType(BillingClient.ProductType.SUBS)
                        .build(),
                (billingResult, purchases) -> {
                    if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        for (Purchase p : purchases) {
                            handlePurchase(p);
                            if (isActive(p, PRODUCT_YEAR)) owned[1] = true;
                        }
                        ok[1] = true;
                    } else {
                        Log.w(TAG, "SUBS purchase query failed: "
                                + billingResult.getDebugMessage());
                    }
                    done[1] = true;
                    if (done[0]) combineRestore(owned, ok);
                });
    }

    /** Applies the combined result of the two purchase queries. */
    private void combineRestore(boolean[] owned, boolean[] ok) {
        if (owned[0] || owned[1]) {
            applyEntitlement(true);
        } else if (ok[0] && ok[1]) {
            // Both queries answered and neither found an active purchase, so
            // this is a real lapse rather than a lookup that fell over.
            applyEntitlement(false);
        } else {
            // Nothing owned, but at least one query never answered -- leave the
            // existing flag alone rather than revoking on missing data. The
            // next restorePurchases() (next launch, or an ITEM_ALREADY_OWNED)
            // settles it.
            Log.w(TAG, "Purchase query incomplete; leaving entitlement unchanged");
        }
    }

    private static boolean isActive(Purchase p, String productId) {
        return p.getProducts().contains(productId)
                && p.getPurchaseState() == Purchase.PurchaseState.PURCHASED;
    }

    private void applyEntitlement(boolean adsRemoved) {
        AdsManager.setAdsRemoved(mActivity, adsRemoved);
        Log.d(TAG, "Entitlement resolved: adsRemoved=" + adsRemoved);
    }

    private void queryProductDetails() {
        // One query per product type -- Play rejects a single query mixing
        // INAPP and SUBS products.
        queryDetailsFor(PRODUCT_FOREVER, BillingClient.ProductType.INAPP);
        queryDetailsFor(PRODUCT_YEAR,    BillingClient.ProductType.SUBS);
    }

    private void queryDetailsFor(String productId, String productType) {
        QueryProductDetailsParams params = QueryProductDetailsParams.newBuilder()
                .setProductList(Collections.singletonList(
                        QueryProductDetailsParams.Product.newBuilder()
                                .setProductId(productId)
                                .setProductType(productType)
                                .build()))
                .build();
        mBillingClient.queryProductDetailsAsync(params, (billingResult, productDetailsList) -> {
            if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK
                    && !productDetailsList.isEmpty()) {
                ProductDetails details = productDetailsList.get(0);
                mProductDetails.put(productId, details);
                Log.d(TAG, "Product loaded: " + productId + " (" + details.getName() + ")");
            } else {
                Log.w(TAG, "Product not available: " + productId
                        + " (" + billingResult.getDebugMessage() + ")");
            }
        });
    }

    /**
     * Returns the localized price for a product, or "" if Play has not
     * answered yet. Prices are set in Play Console and vary by country, so the
     * UI must never hardcode them -- showing "$5" to somebody billed in euros
     * would be wrong, and in several jurisdictions displaying a price other
     * than the one charged is its own problem.
     */
    public static String getPrice(String productId) {
        return sInstance == null ? "" : sInstance.priceFor(productId);
    }

    private String priceFor(String productId) {
        ProductDetails details = mProductDetails.get(productId);
        if (details == null) return "";
        if (PRODUCT_FOREVER.equals(productId)) {
            ProductDetails.OneTimePurchaseOfferDetails otp =
                    details.getOneTimePurchaseOfferDetails();
            return otp == null ? "" : otp.getFormattedPrice();
        }
        List<ProductDetails.SubscriptionOfferDetails> offers =
                details.getSubscriptionOfferDetails();
        if (offers == null || offers.isEmpty()) return "";
        List<ProductDetails.PricingPhase> phases =
                offers.get(0).getPricingPhases().getPricingPhaseList();
        if (phases.isEmpty()) return "";
        // The last phase is the recurring one. Phases are ordered as the buyer
        // meets them, so phase 0 is the free trial or discounted introductory
        // period whenever one is configured in Play Console -- and taking it
        // would print "$0.00" next to a subscription that actually renews at
        // full price, which is exactly what this method's contract forbids.
        // Play Console needs no app change to add such an offer, so this must
        // hold without anyone remembering to come back here.
        return phases.get(phases.size() - 1).getFormattedPrice();
    }

    private void handlePurchase(Purchase purchase) {
        boolean ours = purchase.getProducts().contains(PRODUCT_FOREVER)
                    || purchase.getProducts().contains(PRODUCT_YEAR);
        if (!ours) return;
        if (purchase.getPurchaseState() != Purchase.PurchaseState.PURCHASED) return;

        // Acknowledge if not already done. Play auto-refunds an unacknowledged
        // purchase after three days, so this is not optional -- it applies to
        // subscriptions exactly as it does to one-time products.
        if (!purchase.isAcknowledged()) {
            AcknowledgePurchaseParams ackParams = AcknowledgePurchaseParams.newBuilder()
                    .setPurchaseToken(purchase.getPurchaseToken())
                    .build();
            mBillingClient.acknowledgePurchase(ackParams, ackResult ->
                    Log.d(TAG, "Purchase acknowledged: " + ackResult.getResponseCode()));
        }
    }
}
