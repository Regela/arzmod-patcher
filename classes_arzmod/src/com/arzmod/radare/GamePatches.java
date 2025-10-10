package com.arzmod.radare;

import android.content.Intent;
import android.content.Context;
import android.os.Build;
import java.util.Objects;
import com.arizona.game.GTASA;
import android.app.AlertDialog;
import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.MotionEvent;
import com.arzmod.radare.SampQuery;
import com.arzmod.radare.InitGamePatch;
import com.arzmod.radare.Timers;
import com.arzmod.radare.AppContext;
import ru.mrlargha.commonui.elements.hud.presentation.Hud;
import ru.mrlargha.commonui.R;
import com.arizona.game.BuildConfig;
import com.squareup.picasso.Picasso;
import com.miami.game.feature.download.dialog.ui.connection.ConnectionHolder;

public class GamePatches {
    public static native void nativeOnTouch(int type, int id, double x, double y);


    public static void installTouchForwarder(final Activity activity) {
        final View root = activity.getWindow().getDecorView();
        root.setOnTouchListener(new View.OnTouchListener() {
            @Override public boolean onTouch(View v, MotionEvent e) {
                final int idx = e.getActionIndex();
                GamePatches.nativeOnTouch(
                    e.getActionMasked(),
                    e.getPointerId(idx),
                    e.getX(idx),
                    e.getY(idx)
                );
                return false;
            }
        });
    }

    public static boolean isHudVisible(Hud hud, boolean isDefaultHud) {
        if(Objects.equals(Build.CPU_ABI, "arm64-v8a") && SettingsPatch.getSettingsKeyInt(SettingsPatch.HUD_TYPE) != 3 && !SettingsPatch.getSettingsKeyValue(SettingsPatch.IS_UNITY_ELEMENTS)) return false;
        if(SettingsPatch.getSettingsKeyValue(SettingsPatch.IS_UNITY_ELEMENTS) && SettingsPatch.getSettingsKeyInt(SettingsPatch.HUD_TYPE) != 3) {
            hud.binding.leftMenu.getRoot().setVisibility(View.INVISIBLE);
            hud.binding.hudContainer.setVisibility(View.INVISIBLE);
            hud.binding.hudServerInfoContainer.setVisibility(View.VISIBLE);
            int[] server = InitGamePatch.getServer();
            if(server != null) hud.installHud(0, server[1], server[0] == 0 ? 1 : server[0] == 1 ? 2 : server[0] == 2 ? 4 : server[0] == 4 ? 0 : server[0], 0);
            return true;
        }
        return isDefaultHud;
    }

    public static void updateHudShield(Hud hud)
    {
        if(InitGamePatch.isCustomServer())
        {
            String[] ipPort = InitGamePatch.getCustomServer();
            if (ipPort != null) {
                String ip = ipPort[0];
                int port = Integer.parseInt(ipPort[1]);
                Log.d("arzmod-gamepatches-module", "ip: " + ip + " port: " + port);
                SampQuery.getServerName(ip, port, new SampQuery.ServerNameCallback() {
                    @Override
                    public void onResult(String serverName) {
                        new Handler(Looper.getMainLooper()).post(new Runnable() {
                            @Override
                            public void run() {
                                Log.d("arzmod-gamepatches-module", "serverName: " + serverName);
                                hud.binding.hudServerShieldSite.setText("arzmod.com");
                                hud.binding.hudServerShieldName.setText(serverName);
                                Picasso.get().load("https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcQIclHyzkxY9JTTGH7uP-vTGI2-wcIBlTh5dA&s").placeholder(R.drawable.logo_phoenix).into(hud.binding.hudServerShieldLogo);
                            }
                        });
                    }
                });
            }
        }
    }

    public static int checkConnectionTimer = 0;
    public static int serverNotRespond = 0;
    public static void onSetConnectState(String state)
    {
        if(state.equals("Подключение к игре..."))
        {
            if(!BuildConfig.IS_ARIZONA) return; // TODO: fix rodina check (rodina don't send connect state, and i found this while i making a release)
            checkConnectionTimer = Timers.startTimer(30000, new Timers.TimerCallback() {
                @Override
                public void onTimerTick(int timerId, int tickCount, long currentTime) {
                    if(tickCount == 1) return;
                    Activity activity = AppContext.getActivity();
                    if (activity == null) {
                        Log.e("arzmod-gamepatches-module", "Context and message cannot be null");
                        return;
                    }
                    Handler mainHandler = new Handler(Looper.getMainLooper());
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                new AlertDialog.Builder(activity)
                                    .setTitle("Игра не отвечает")
                                    .setMessage("Игра не отправила запрос на подключение к серверу в течении 30 секунд\nВозможные решения:\n1. Проверьте свои скрипты. Попробуйте запустить игру без скриптов, возможно они ломают игру на этапе запуска\n2. Попробуйте сбросить настройки ARZMOD в правом верхнем углу настроек ARZMOD\n3. Попробуйте запустить проверку файлов в настройках лаунчера и обновить файлы игры\n4. Проверьте наличия интернет подключения\n5. Попробуйте перезапустить игру\n\nЕсли вам ничего из этого не помогает - поделитесь своей проблемой приложив логи в t.me/cleodis")
                                    .setPositiveButton("Выход", new android.content.DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick(android.content.DialogInterface dialog, int which) {
                                            activity.finish();
                                        }
                                    })
                                    .show();
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    });
                    Timers.stopTimer(timerId);
                }
            });
        }
        else
        {
            if(checkConnectionTimer != 0) {
                Timers.stopTimer(checkConnectionTimer);
                checkConnectionTimer = 0;
            }
            if(state.equals("Сервер не ответил. Повторная попытка..."))
            {
                serverNotRespond = serverNotRespond + 1;
                if(serverNotRespond > 3)
                {
                    Activity activity = AppContext.getActivity();
                    if (activity == null) {
                        Log.e("arzmod-gamepatches-module", "Context and message cannot be null");
                        return;
                    }
                    Handler mainHandler = new Handler(Looper.getMainLooper());
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                String ip = InitGamePatch.isCustomServer() ? InitGamePatch.getCustomServer()[0] : getCurrentServerIP();

                                sendAsyncPingRequests(ip, 825);
                                sendAsyncPingRequests(ip, 80);
                                
                                new AlertDialog.Builder(activity)
                                    .setTitle("Сервер не отвечает")
                                    .setMessage("Сервер (" + ip + ") не отвечает на запросы на подключение в течении 3 попыток\nВозможные решения:\n1. Проверьте свои скрипты. Попробуйте запустить игру без скриптов, возможно они ломают игру на этапе подключения\n2. Попробуйте сбросить настройки ARZMOD в правом верхнем углу настроек ARZMOD\n3. Попробуйте перезапустить игру\n4. Попробуйте подождать еще некоторое время (мы отправили PING на сервер, возможно теперь он ответит)\n\nЕсли вам ничего из этого не помогает - поделитесь своей проблемой приложив логи в t.me/cleodis")
                                    .setPositiveButton("ОК", new android.content.DialogInterface.OnClickListener() {
                                        @Override
                                        public void onClick(android.content.DialogInterface dialog, int which) {
                                            dialog.dismiss();
                                        }
                                    })
                                    .show();
                            } catch (Exception e) {
                                e.printStackTrace();
                            }
                        }
                    });
                    serverNotRespond = 0;
                }
            }
        }
    }


    private static void sendAsyncPingRequests(String ip, int port) {
        if (ip == null || ip.isEmpty()) {
            Log.w("arzmod-gamepatches-module", "IP is null or empty, skipping ping requests");
            return;
        }

        String baseUrl = "http://" + ip + (port > 0 ? ":" + port : "") + "/";
        Log.d("arzmod-gamepatches-module", "Sending 5 async PING requests to: " + baseUrl);

        java.util.concurrent.ExecutorService executor = java.util.concurrent.Executors.newFixedThreadPool(5);
        java.util.concurrent.CountDownLatch latch = new java.util.concurrent.CountDownLatch(5);

        for (int i = 0; i < 5; i++) {
            final int requestNumber = i + 1;
            executor.submit(new Runnable() {
                @Override
                public void run() {
                    try {
                        sendPingRequest(baseUrl, requestNumber);
                    } catch (Exception e) {
                        Log.e("arzmod-gamepatches-module", "Error in ping request " + requestNumber + ": " + e.getMessage());
                    } finally {
                        latch.countDown();
                    }
                }
            });
        }

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    boolean completed = latch.await(10, java.util.concurrent.TimeUnit.SECONDS);
                    if (completed) {
                        Log.d("arzmod-gamepatches-module", "All 5 PING requests completed");
                    } else {
                        Log.w("arzmod-gamepatches-module", "Some PING requests timed out");
                    }
                } catch (InterruptedException e) {
                    Log.w("arzmod-gamepatches-module", "PING requests interrupted: " + e.getMessage());
                } finally {
                    executor.shutdown();
                }
            }
        }).start();
    }


    private static void sendPingRequest(String baseUrl, int requestNumber) {
        java.net.HttpURLConnection connection = null;
        try {
            java.net.URL url = new java.net.URL(baseUrl);
            connection = (java.net.HttpURLConnection) url.openConnection();
            
            connection.setRequestMethod("GET");
            connection.setConnectTimeout(3000);
            connection.setReadTimeout(5000);  
            connection.setRequestProperty("User-Agent", "okhttp/" + requestNumber);
            connection.setRequestProperty("Accept", "*/*");
            
            long startTime = System.currentTimeMillis();
            int responseCode = connection.getResponseCode();
            long endTime = System.currentTimeMillis();
            long responseTime = endTime - startTime;
            
            Log.d("arzmod-gamepatches-module", 
                "PING request " + requestNumber + " completed: " + 
                "HTTP " + responseCode + " in " + responseTime + "ms");
                
        } catch (java.net.SocketTimeoutException e) {
            Log.w("arzmod-gamepatches-module", "PING request " + requestNumber + " timed out");
        } catch (java.net.ConnectException e) {
            Log.w("arzmod-gamepatches-module", "PING request " + requestNumber + " connection failed: " + e.getMessage());
        } catch (Exception e) {
            Log.e("arzmod-gamepatches-module", "PING request " + requestNumber + " error: " + e.getMessage());
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    public static String getCurrentServerIP() {
        try {
            int[] server = InitGamePatch.getServer();
            String fileName = (server[0] > 1) ? "server_mobile.json" : "server_desktop.json";
            
            Context context = AppContext.getContext();
            if (context == null) {
                Log.e("arzmod-gamepatches-module", "Context is null (getCurrentServerIP)");
                return null;
            }
            
            java.io.InputStream inputStream = context.getAssets().open(fileName);
            java.util.Scanner scanner = new java.util.Scanner(inputStream).useDelimiter("\\A");
            String jsonString = scanner.hasNext() ? scanner.next() : "";
            scanner.close();
            inputStream.close();
            
            org.json.JSONArray serversArray = new org.json.JSONArray(jsonString);
            
            int targetServerId = server[1];
            for (int i = 0; i < serversArray.length(); i++) {
                org.json.JSONObject serverObj = serversArray.getJSONObject(i);
                int serverNumber = serverObj.getInt("number");
                
                if (serverNumber == targetServerId) {
                    String ip = serverObj.getString("ip");
                    Log.i("arzmod-gamepatches-module", "Found server IP: " + ip + " for server ID: " + targetServerId);
                    return ip;
                }
            }
            
            Log.w("arzmod-gamepatches-module", "Server with ID " + targetServerId + " not found in " + fileName);
            return null;
            
        } catch (Exception e) {
            Log.e("arzmod-gamepatches-module", "Failed to get server IP from JSON: " + e.getMessage());
            return null;
        }
    }

}