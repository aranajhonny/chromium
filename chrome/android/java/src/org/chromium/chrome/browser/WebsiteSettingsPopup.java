// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.provider.Browser;
import android.text.Html;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.content.browser.WebContentsObserverAndroid;
import org.chromium.content_public.browser.WebContents;

import java.net.URISyntaxException;

/**
 * Java side of Android implementation of the website settings UI.
 */
public class WebsiteSettingsPopup implements OnClickListener {
    private static final String HELP_URL =
            "http://www.google.com/support/chrome/bin/answer.py?answer=95617";
    private final Context mContext;
    private final Dialog mDialog;
    private final LinearLayout mContainer;
    private final WebContents mWebContents;
    private final int mPadding;
    private TextView mCertificateViewer, mMoreInfoLink;
    private String mLinkUrl;

    private WebsiteSettingsPopup(Context context, WebContents webContents) {
        mContext = context;
        mWebContents = webContents;

        mContainer = new LinearLayout(mContext);
        mContainer.setOrientation(LinearLayout.VERTICAL);
        mPadding = (int) context.getResources().getDimension(R.dimen.certificate_viewer_padding);
        mContainer.setPadding(mPadding, 0, mPadding, 0);

        mDialog = new Dialog(mContext);
        mDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        mDialog.setCanceledOnTouchOutside(true);
        // This needs to come after other member initialization.
        final long nativeWebsiteSettingsPopup = nativeInit(this, webContents);
        final WebContentsObserverAndroid webContentsObserver =
                new WebContentsObserverAndroid(mWebContents) {
            @Override
            public void navigationEntryCommitted() {
                // If a navigation is committed (e.g. from in-page redirect), the data we're
                // showing is stale so dismiss the dialog.
                mDialog.dismiss();
            }
        };
        mDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                assert nativeWebsiteSettingsPopup != 0;
                webContentsObserver.detachFromWebContents();
                nativeDestroy(nativeWebsiteSettingsPopup);
            }
        });
    }

    /** Adds a section, which contains an icon, a headline, and a description. */
    @CalledByNative
    private void addSection(int enumeratedIconId, String headline, String description) {
        View section = LayoutInflater.from(mContext).inflate(R.layout.website_settings, null);
        ImageView i = (ImageView) section.findViewById(R.id.website_settings_icon);
        int drawableId = ResourceId.mapToDrawableId(enumeratedIconId);
        i.setImageResource(drawableId);

        TextView h = (TextView) section.findViewById(R.id.website_settings_headline);
        h.setText(headline);
        if (TextUtils.isEmpty(headline)) h.setVisibility(View.GONE);

        TextView d = (TextView) section.findViewById(R.id.website_settings_description);
        d.setText(description);
        if (TextUtils.isEmpty(description)) d.setVisibility(View.GONE);

        mContainer.addView(section);
    }

    /** Adds a horizontal dividing line to separate sections. */
    @CalledByNative
    private void addDivider() {
        View divider = new View(mContext);
        final int dividerHeight = (int) (2 * mContext.getResources().getDisplayMetrics().density);
        divider.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, dividerHeight));
        divider.setBackgroundColor(Color.GRAY);
        mContainer.addView(divider);
    }

    @CalledByNative
    private void setCertificateViewer(String label) {
        assert mCertificateViewer == null;
        mCertificateViewer =  new TextView(mContext);
        mCertificateViewer.setText(Html.fromHtml("<a href='#'>" + label + "</a>"));
        mCertificateViewer.setOnClickListener(this);
        mCertificateViewer.setPadding(0, 0, 0, mPadding);
        mContainer.addView(mCertificateViewer);
    }

    @CalledByNative
    private void addMoreInfoLink(String linkText) {
        addUrl(linkText, HELP_URL);
    }

    /** Adds a section containing a description and a hyperlink. */
    private void addUrl(String label, String url) {
        mMoreInfoLink = new TextView(mContext);
        mLinkUrl = url;
        mMoreInfoLink.setText(Html.fromHtml("<a href='#'>" + label + "</a>"));
        mMoreInfoLink.setPadding(0, mPadding, 0, mPadding);
        mMoreInfoLink.setOnClickListener(this);
        mContainer.addView(mMoreInfoLink);
    }

    /** Displays the WebsiteSettingsPopup. */
    @CalledByNative
    private void showDialog() {
        ScrollView scrollView = new ScrollView(mContext);
        scrollView.addView(mContainer);
        mDialog.addContentView(scrollView,
                new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));
        mDialog.show();
    }

    @Override
    public void onClick(View v) {
        mDialog.dismiss();
        if (mCertificateViewer == v) {
            byte[][] certChain = nativeGetCertificateChain(mWebContents);
            if (certChain == null) {
                // The WebContents may have been destroyed/invalidated. If so,
                // ignore this request.
                return;
            }
            CertificateViewer.showCertificateChain(mContext, certChain);
        } else if (mMoreInfoLink == v) {
            try {
                Intent i = Intent.parseUri(mLinkUrl, Intent.URI_INTENT_SCHEME);
                i.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
                i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
                mContext.startActivity(i);
            } catch (URISyntaxException ex) {}
        }
    }

    /**
     * Shows a WebsiteSettings dialog for the provided WebContents.
     *
     * The popup adds itself to the view hierarchy which owns the reference while it's
     * visible.
     *
     * @param context Context which is used for launching a dialog.
     * @param webContents The WebContents for which to show Website information. This
     *         information is retrieved for the visible entry.
     */
    @SuppressWarnings("unused")
    public static void show(Context context, WebContents webContents) {
        new WebsiteSettingsPopup(context, webContents);
    }

    private static native long nativeInit(WebsiteSettingsPopup popup, WebContents webContents);
    private native void nativeDestroy(long nativeWebsiteSettingsPopupAndroid);
    private native byte[][] nativeGetCertificateChain(WebContents webContents);
}