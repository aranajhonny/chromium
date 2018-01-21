// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.PopupWindow;

/**
 * Paste popup implementation based on TextView.PastePopupMenu.
 */
public class PastePopupMenu implements OnClickListener {
    private final View mParent;
    private final PastePopupMenuDelegate mDelegate;
    private final Context mContext;
    private final PopupWindow mContainer;
    private int mRawPositionX;
    private int mRawPositionY;
    private int mPositionX;
    private int mPositionY;
    private final View[] mPasteViews;
    private final int[] mPasteViewLayouts;
    private final int mLineOffsetY;
    private final int mWidthOffsetX;

    /**
     * Provider of paste functionality for the given popup.
     */
    public interface PastePopupMenuDelegate {
        /**
         * Called to initiate a paste after the popup has been tapped.
         */
        void paste();

        /**
         * @return true iff content can be pasted to a focused editable region.
         */
        boolean canPaste();
    }

    public PastePopupMenu(View parent, PastePopupMenuDelegate delegate) {
        mParent = parent;
        mDelegate = delegate;
        mContext = parent.getContext();
        mContainer = new PopupWindow(mContext, null,
                android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);
        mContainer.setAnimationStyle(0);

        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);

        final int[] POPUP_LAYOUT_ATTRS = {
            android.R.attr.textEditPasteWindowLayout,
            android.R.attr.textEditNoPasteWindowLayout,
            android.R.attr.textEditSidePasteWindowLayout,
            android.R.attr.textEditSideNoPasteWindowLayout,
        };

        mPasteViews = new View[POPUP_LAYOUT_ATTRS.length];
        mPasteViewLayouts = new int[POPUP_LAYOUT_ATTRS.length];

        TypedArray attrs = mContext.getTheme().obtainStyledAttributes(POPUP_LAYOUT_ATTRS);
        for (int i = 0; i < attrs.length(); ++i) {
            mPasteViewLayouts[i] = attrs.getResourceId(attrs.getIndex(i), 0);
        }
        attrs.recycle();

        // Convert line offset dips to pixels.
        mLineOffsetY = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                5.0f, mContext.getResources().getDisplayMetrics());
        mWidthOffsetX = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                30.0f, mContext.getResources().getDisplayMetrics());
    }

    /**
     * Shows the paste popup at an appropriate location relative to the specified position.
     */
    public void showAt(int x, int y) {
        if (!canPaste()) return;
        updateContent(true);
        positionAt(x, y);
    }

    /**
     * Hides the paste popup.
     */
    public void hide() {
        mContainer.dismiss();
    }

    /**
     * @return Whether the popup is active and showing.
     */
    public boolean isShowing() {
        return mContainer.isShowing();
    }

    @Override
    public void onClick(View v) {
        if (canPaste()) {
            paste();
        }
        hide();
    }

    private void positionAt(int x, int y) {
        if (mRawPositionX == x && mRawPositionY == y && isShowing()) return;
        mRawPositionX = x;
        mRawPositionY = y;

        View contentView = mContainer.getContentView();
        int width = contentView.getMeasuredWidth();
        int height = contentView.getMeasuredHeight();

        mPositionX = (int) (x - width / 2.0f);
        mPositionY = y - height - mLineOffsetY;

        final int[] coords = new int[2];
        mParent.getLocationInWindow(coords);
        coords[0] += mPositionX;
        coords[1] += mPositionY;

        final int screenWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        if (coords[1] < 0) {
            updateContent(false);
            // Update dimensions from new view
            contentView = mContainer.getContentView();
            width = contentView.getMeasuredWidth();
            height = contentView.getMeasuredHeight();

            // Vertical clipping, move under edited line and to the side of insertion cursor
            // TODO bottom clipping in case there is no system bar
            coords[1] += height;
            coords[1] += mLineOffsetY;

            // Move to right hand side of insertion cursor by default. TODO RTL text.
            final int handleHalfWidth = mWidthOffsetX / 2;

            if (x + width < screenWidth) {
                coords[0] += handleHalfWidth + width / 2;
            } else {
                coords[0] -= handleHalfWidth + width / 2;
            }
        } else {
            // Horizontal clipping
            coords[0] = Math.max(0, coords[0]);
            coords[0] = Math.min(screenWidth - width, coords[0]);
        }

        mContainer.showAtLocation(mParent, Gravity.NO_GRAVITY, coords[0], coords[1]);
    }

    private int viewIndex(boolean onTop) {
        return (onTop ? 0 : 1 << 1) + (canPaste() ? 0 : 1 << 0);
    }

    private void updateContent(boolean onTop) {
        final int viewIndex = viewIndex(onTop);
        View view = mPasteViews[viewIndex];

        if (view == null) {
            final int layout = mPasteViewLayouts[viewIndex];
            LayoutInflater inflater = (LayoutInflater) mContext.
                getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            if (inflater != null) {
                view = inflater.inflate(layout, null);
            }

            if (view == null) {
                throw new IllegalArgumentException("Unable to inflate TextEdit paste window");
            }

            final int size = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
            view.setLayoutParams(new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT));
            view.measure(size, size);

            view.setOnClickListener(this);

            mPasteViews[viewIndex] = view;
        }

        mContainer.setContentView(view);
    }

    private boolean canPaste() {
        return mDelegate.canPaste();
    }

    private void paste() {
        mDelegate.paste();
    }
}
