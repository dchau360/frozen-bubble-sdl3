/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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
import java.util.List;

import androidx.annotation.NonNull;

/**
 * Handles Google Play Billing for the "Remove Ads" one-time purchase.
 *
 * Product ID to create in Google Play Console: "remove_ads"  (one-time purchase)
 *
 * Usage:
 *   BillingManager mgr = new BillingManager(activity);
 *   mgr.launchPurchaseFlow();  // starts the purchase UI
 *   mgr.restorePurchases();    // call on startup to restore prior purchases
 *   mgr.destroy();             // call in onDestroy()
 */
public class BillingManager implements PurchasesUpdatedListener {
    private static final String TAG        = "FBubble.Billing";
    public  static final String PRODUCT_ID = "remove_ads";

    private final Activity      mActivity;
    private       BillingClient mBillingClient;
    private       ProductDetails mProductDetails = null;

    public BillingManager(Activity activity) {
        mActivity = activity;
        mBillingClient = BillingClient.newBuilder(activity)
                .setListener(this)
                .enablePendingPurchases()
                .build();
        connectAndRestore();
    }

    /** Start the Play Store purchase UI for "remove_ads". */
    public void launchPurchaseFlow() {
        if (mProductDetails == null) {
            Log.w(TAG, "Product details not loaded yet — retrying connect");
            connectAndRestore();
            return;
        }
        BillingFlowParams params = BillingFlowParams.newBuilder()
                .setProductDetailsParamsList(Collections.singletonList(
                        BillingFlowParams.ProductDetailsParams.newBuilder()
                                .setProductDetails(mProductDetails)
                                .build()))
                .build();
        mBillingClient.launchBillingFlow(mActivity, params);
    }

    /** Call on Activity.onDestroy() */
    public void destroy() {
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
            for (Purchase p : purchases) handlePurchase(p);
        } else if (result.getResponseCode() == BillingClient.BillingResponseCode.USER_CANCELED) {
            Log.d(TAG, "User cancelled purchase");
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

    private void restorePurchases() {
        mBillingClient.queryPurchasesAsync(
                QueryPurchasesParams.newBuilder()
                        .setProductType(BillingClient.ProductType.INAPP)
                        .build(),
                (billingResult, purchases) -> {
                    if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        for (Purchase p : purchases) handlePurchase(p);
                    }
                });
    }

    private void queryProductDetails() {
        QueryProductDetailsParams params = QueryProductDetailsParams.newBuilder()
                .setProductList(Collections.singletonList(
                        QueryProductDetailsParams.Product.newBuilder()
                                .setProductId(PRODUCT_ID)
                                .setProductType(BillingClient.ProductType.INAPP)
                                .build()))
                .build();
        mBillingClient.queryProductDetailsAsync(params, (billingResult, productDetailsList) -> {
            if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK
                    && !productDetailsList.isEmpty()) {
                mProductDetails = productDetailsList.get(0);
                Log.d(TAG, "Product loaded: " + mProductDetails.getName());
            }
        });
    }

    private void handlePurchase(Purchase purchase) {
        if (!purchase.getProducts().contains(PRODUCT_ID)) return;
        if (purchase.getPurchaseState() != Purchase.PurchaseState.PURCHASED) return;

        // Grant entitlement
        AdsManager.setAdsRemoved(mActivity);

        // Acknowledge if not already done
        if (!purchase.isAcknowledged()) {
            AcknowledgePurchaseParams ackParams = AcknowledgePurchaseParams.newBuilder()
                    .setPurchaseToken(purchase.getPurchaseToken())
                    .build();
            mBillingClient.acknowledgePurchase(ackParams, ackResult ->
                    Log.d(TAG, "Purchase acknowledged: " + ackResult.getResponseCode()));
        }
    }
}
