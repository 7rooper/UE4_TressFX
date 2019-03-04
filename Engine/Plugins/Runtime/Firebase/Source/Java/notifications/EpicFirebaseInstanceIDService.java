package com.epicgames.ue4.notifications;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.epicgames.ue4.Logger;
import com.google.firebase.FirebaseApp;
import com.google.firebase.iid.FirebaseInstanceId;
import com.google.firebase.iid.FirebaseInstanceIdService;

import static com.epicgames.ue4.GameApplication.MESSAGING_CONFIG;

public class EpicFirebaseInstanceIDService extends FirebaseInstanceIdService {
	private static final Logger Log = new Logger("UE4-" + EpicFirebaseInstanceIDService.class.getSimpleName());
	private static final String PREFS_FILE_FIREBASE = "com.epicgames.firebase";
	private static final String KEY_FIREBASE_TOKEN = "firebasetoken";
	private static final String KEY_IS_UPDATED_TOKEN = "isUpdatedToken";
	private static final String KEY_IS_REGISTERED = "isRegistered";

	@Override
	public void onTokenRefresh() {
		String firebaseToken = getFirebaseInstanceId().getToken();
		Log.debug("Refreshed Firebase token: " + firebaseToken);
		if (TextUtils.isEmpty(firebaseToken)) {
			Log.error("Firebase token is empty or null");
		} else {
			saveFirebaseToken(this, firebaseToken);
		}
	}

	private static FirebaseInstanceId getFirebaseInstanceId() {
		FirebaseApp app = FirebaseApp.getInstance(MESSAGING_CONFIG);
		return FirebaseInstanceId.getInstance(app);
	}
	
	private static void saveFirebaseToken(@NonNull Context context, @NonNull String firebaseToken) {
		Log.debug("Firebase token to save : " + firebaseToken);
		String storedToken = getFirebaseTokenFromCache(context);
		boolean isUpdatedToken = !TextUtils.isEmpty(storedToken);
		SharedPreferences sharedPreferences = context.getSharedPreferences(PREFS_FILE_FIREBASE, Context.MODE_PRIVATE);
		SharedPreferences.Editor editor = sharedPreferences.edit();
		Log.debug("Firebase token isUpdated : " + isUpdatedToken);
		editor.putBoolean(KEY_IS_UPDATED_TOKEN, isUpdatedToken).apply();
		editor.putBoolean(KEY_IS_REGISTERED, false).apply();
		editor.putString(KEY_FIREBASE_TOKEN, firebaseToken).apply();
	}

	@SuppressWarnings("unused")
	static boolean isFirebaseTokenUpdated(@NonNull Context context) {
		SharedPreferences sharedPreferences = context.getSharedPreferences(PREFS_FILE_FIREBASE, Context.MODE_PRIVATE);
		boolean isUpdated = sharedPreferences.getBoolean(KEY_IS_UPDATED_TOKEN, true);
		Log.debug("Firebase token isUpdatedToken is " + isUpdated);
		return isUpdated;
	}

	public static boolean isFirebaseTokenRegistered(@NonNull Context context) {
		SharedPreferences sharedPreferences = context.getSharedPreferences(PREFS_FILE_FIREBASE, Context.MODE_PRIVATE);
		return sharedPreferences.getBoolean(KEY_IS_REGISTERED, false);
	}

	public static void setFirebaseTokenRegistered(@NonNull Context context, boolean isRegistered) {
		Log.debug("Firebase token isRegistered setting to " + isRegistered);
		SharedPreferences sharedPreferences = context.getSharedPreferences(PREFS_FILE_FIREBASE, Context.MODE_PRIVATE);
		SharedPreferences.Editor editor = sharedPreferences.edit();
		editor.putBoolean(KEY_IS_REGISTERED, isRegistered);
		editor.apply();
	}

	private static String getFirebaseTokenFromCache(@NonNull Context context) {
		SharedPreferences sharedPreferences = context.getSharedPreferences(PREFS_FILE_FIREBASE, Context.MODE_PRIVATE);
		return sharedPreferences.getString(KEY_FIREBASE_TOKEN, null);
	}
	
	@Nullable
	public static String getFirebaseToken(@NonNull Context context) {
		String token = getFirebaseTokenFromCache(context);
		Log.debug("Firebase token retrieved from cache: " + token);
		if(TextUtils.isEmpty(token)) {
			// handle edge case where we missed onTokenRefresh - ex. App Upgrade
			token = getFirebaseInstanceId().getToken();
			if(!TextUtils.isEmpty(token)) {
				Log.debug("Firebase token retrieved from Firebase: " + token);
				saveFirebaseToken(context, token);
			}
		}
		return token;
	}
}
