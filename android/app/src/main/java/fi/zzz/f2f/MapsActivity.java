package fi.zzz.f2f;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.support.annotation.NonNull;
import android.support.design.widget.NavigationView;
import android.os.Bundle;
import com.google.android.gms.maps.CameraUpdateFactory;
import com.google.android.gms.maps.GoogleMap;
import com.google.android.gms.maps.OnMapReadyCallback;
import com.google.android.gms.maps.SupportMapFragment;
import com.google.android.gms.maps.model.BitmapDescriptorFactory;
import com.google.android.gms.maps.model.LatLng;
import com.google.android.gms.maps.model.Marker;
import com.google.android.gms.maps.model.MarkerOptions;
import android.support.v4.content.ContextCompat;
import android.support.v4.view.GravityCompat;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.text.InputType;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import java.util.HashMap;
import java.util.HashSet;

public class MapsActivity extends AppCompatActivity implements OnMapReadyCallback {

    public static MapsActivity last;

    private GoogleMap mMap;
    private DrawerLayout mDrawerLayout;
    private HashMap<Integer, Marker> markers;
    private Marker my_marker;

    private static final String TAG = "F2F";
    private static final int REQUEST_PERMISSION_LOCATION = 255; // int should be between 0 and 255


    class IncomingHandler extends Handler {
        @Override
        public void handleMessage(Message msg) {
            Bundle bundle = msg.getData();
            switch (msg.what) {
                case AppService.STARTONBOARDING:
                    startOnboarding();
                    break;

                case AppService.POSTREFRESHGUI:
                    HashMap<String, Channel> channels = (HashMap<String, Channel>)bundle.getSerializable("channels");
                    HashSet<String> my_channels = (HashSet<String>)bundle.getSerializable("my_channels");
                    final String active_channel = (String)bundle.getSerializable("active_channel");
                    HashMap<Integer, User> users = (HashMap<Integer, User>)bundle.getSerializable("users");
                    refreshGui(channels, my_channels, active_channel, users);
                    break;

                case AppService.POSTSETTITLE:
                    String title = (String)bundle.getSerializable("title");
                    Toolbar toolbar = findViewById(R.id.toolbar);
                    toolbar.setTitle(title);
                    break;

                case AppService.LOCATION:
                    int user_id = (int)bundle.getSerializable("user_id");
                    double lon = (double)bundle.getSerializable("lon");
                    double lat = (double)bundle.getSerializable("lat");
                    Marker m = markers.get(user_id);
                    if (m != null)
                        m.setPosition(new LatLng(lat, lon));
                    break;

                case AppService.PROFILEIMAGE:
                    user_id = (int)bundle.getSerializable("user_id");
                    BitmapDataObject bm = (BitmapDataObject)bundle.getSerializable("profile_image");
                    m = markers.get(user_id);
                    if (m != null)
                        m.setIcon(BitmapDescriptorFactory.fromBitmap(getCroppedBitmap(Bitmap.createScaledBitmap(bm.currentImage, 128, 128, true))));
                    break;

                case AppService.ADDMESSAGES:
                    Channel ch = (Channel)bundle.getSerializable("ch");
                    MessagesActivity.last.addMessages(ch);
                    break;

                default:
                    super.handleMessage(msg);
            }
        }
    }

    /**
     * Target we publish for clients to send messages to IncomingHandler.
     */
    final Messenger mMessenger = new Messenger(new IncomingHandler());
    public Messenger mService = null;

    private ServiceConnection mConnection = new ServiceConnection() {
        public void onServiceConnected(ComponentName className,
                                       IBinder service) {
            mService = new Messenger(service);
            startRegister();
        }

        public void onServiceDisconnected(ComponentName className) {
            mService = null;
        }
    };


    public MapsActivity() {
        last = this;
        markers = new HashMap<>();
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.activity_maps);


            // Start background service
            Intent i = new Intent(this, AppService.class);
            i.putExtra("name", "F2F service");
            bindService(i, mConnection, Context.BIND_AUTO_CREATE);
            startService(i);


            // Obtain the SupportMapFragment and get notified when the map is ready to be used.
            SupportMapFragment mapFragment = (SupportMapFragment) getSupportFragmentManager()
                    .findFragmentById(R.id.map);
            mapFragment.getMapAsync(this);


            // Navigation bar
            mDrawerLayout = findViewById(R.id.drawer_layout);
            NavigationView navigationView = findViewById(R.id.nav_view);
            navigationView.setNavigationItemSelectedListener(
                    new NavigationView.OnNavigationItemSelectedListener() {
                        @Override
                        public boolean onNavigationItemSelected(@NonNull MenuItem menuItem) {
                            // set item as selected to persist highlight
                            //menuItem.setChecked(true);

                            // close drawer when item is tapped
                            mDrawerLayout.closeDrawers();

                            int id = menuItem.getItemId();
                            if (id == R.id.nav_settings) {
                                startSettings();
                            }
                            else if (id == R.id.nav_messages) {
                                startMessages();
                            }
                            // Add code here to update the UI based on the item selected
                            // For example, swap UI fragments here
                            return true;
                        }
                    });


            // Set the toolbar as the action bar
            Toolbar toolbar = findViewById(R.id.toolbar);
            setSupportActionBar(toolbar);
            toolbar.setTitle("@string/activity_maps");
            ActionBar actionbar = getSupportActionBar();
            actionbar.setDisplayHomeAsUpEnabled(true);
            actionbar.setHomeAsUpIndicator(R.drawable.ic_menu);


            // Ask location permissions
            if (ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, REQUEST_PERMISSION_LOCATION);
            }
            else {
                startLocationService();
            }

        }
        catch (java.lang.NullPointerException e) {
            Log.e(TAG, "System error");
            System.exit(1);
        }

    }


    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_PERMISSION_LOCATION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // We now have permission to use the location
                startLocationService();
            }
        }
    }

    void startLocationService() {
        LocationManager locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);

        // Define a listener that responds to location updates
        LocationListener locationListener = new LocationListener() {
            public void onLocationChanged(final Location location) {

                my_marker.setPosition(new LatLng(location.getLatitude(), location.getLongitude()));

                // Called when a new location is found by the network location provider.
                Thread thread = new Thread() {
                    @Override
                    public void run() {
                        sendLocation(location);
                    }
                };
                thread.start();
            }

            public void onStatusChanged(String provider, int status, Bundle extras) {}

            public void onProviderEnabled(String provider) {}

            public void onProviderDisabled(String provider) {}
        };

        // Register the listener with the Location Manager to receive location updates
        locationManager.requestLocationUpdates(LocationManager.NETWORK_PROVIDER, 0, 0, locationListener);
    }


    void startOnboarding() {
        startActivity(new Intent(this, OnboardingActivity.class));
    }

    void startSettings() {
        startActivity(new Intent(this, SettingsActivity.class));
    }

    void startMessages() {
        startActivity(new Intent(this, MessagesActivity.class));
        if (MessagesActivity.last != null)
            MessagesActivity.last.postAddMessages();
    }




    @Override
    public void onMapReady(GoogleMap googleMap) {
        mMap = googleMap;

        // Add a marker in Sydney and move the camera
        LatLng oulu_uni = new LatLng(65.05919, 25.46748);
        my_marker = mMap.addMarker(new MarkerOptions().position(oulu_uni).title("Me"));
        mMap.moveCamera(CameraUpdateFactory.newLatLngZoom(oulu_uni, 16.0f));
        mMap.setIndoorEnabled(true);

    }


    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                mDrawerLayout.openDrawer(GravityCompat.START);
                return true;
        }
        return super.onOptionsItemSelected(item);
    }


    void postSetTitle(final String s) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toolbar toolbar = findViewById(R.id.toolbar);
                if (toolbar != null)
                    toolbar.setTitle(s);
            }
        });
    }

    void postMarkerPosition(final int user_id, final double lat, final double lon) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Marker m = markers.get(user_id);
                if (m != null) {
                    m.setPosition(new LatLng(lat, lon));
                }
            }
        });
    }

    void postProfileImage(final int user_id, final User u) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Marker m = markers.get(user_id);
                if (m != null && u.profile_image != null) {
                    boolean fail = true;
                    try {
                        m.setIcon(BitmapDescriptorFactory.fromBitmap(getCroppedBitmap(Bitmap.createScaledBitmap(u.profile_image, 128, 128, true))));
                        fail = false;
                    }
                    catch (IllegalArgumentException e) {
                        Log.e(TAG, "IllegalArgumentException");
                    }
                    if (fail) {
                        try {
                            m.setIcon(BitmapDescriptorFactory.fromBitmap(getCroppedBitmap(Bitmap.createScaledBitmap(AppService.randomUserImage(), 128, 128, true))));
                        } catch (IllegalArgumentException e) {
                            Log.e(TAG, "IllegalArgumentException");
                        }
                    }
                }
            }
        });
    }


    static public Bitmap getCroppedBitmap(Bitmap bitmap) {
        Bitmap output = Bitmap.createBitmap(bitmap.getWidth(),
                bitmap.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(output);

        final int color = 0xff424242;
        final Paint paint = new Paint();
        final Rect rect = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());

        paint.setAntiAlias(true);
        canvas.drawARGB(0, 0, 0, 0);
        paint.setColor(color);
        canvas.drawCircle(bitmap.getWidth() / 2, bitmap.getHeight() / 2,
                bitmap.getWidth() / 2, paint);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        canvas.drawBitmap(bitmap, rect, rect, paint);
        return output;
    }


    void refreshGui(HashMap<String, Channel> channels, HashSet<String> my_channels, final String active_channel, HashMap<Integer, User> users) {
        final RadioGroup channel_list = findViewById(R.id.channel_list);
        channel_list.removeAllViews();
        int i = 0;
        for (String ch : my_channels) {
            RadioButton rdbtn = new RadioButton(this);
            rdbtn.setId(i++);
            rdbtn.setText(ch);
            channel_list.addView(rdbtn);
            if (active_channel.equals(ch) && !rdbtn.isChecked())
                rdbtn.setChecked(true);
        }
        channel_list.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                RadioButton checkedRadioButton = (RadioButton)group.findViewById(checkedId);
                if (checkedRadioButton != null) {
                    checkedRadioButton.setChecked(true);
                    String ch = checkedRadioButton.getText().toString();
                    if (active_channel.equals(ch) == false)
                        changeChannel(ch);
                }
            }
        });



        Button join_button = findViewById(R.id.join);
        join_button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                joinPrompt();
            }
        });



        Button leave_button = findViewById(R.id.leave);
        leave_button .setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                RadioButton checkedRadioButton = (RadioButton)channel_list.findViewById(channel_list.getCheckedRadioButtonId());
                if (checkedRadioButton != null) {
                    String ch = checkedRadioButton.getText().toString();
                    startLeaveChannel(ch);
                }
            }
        });



        if (!channels.containsKey(active_channel)) { return;}

        Channel ch = channels.get(active_channel);
        if (ch == null) { return;}

        HashSet<Integer> rem_list = new HashSet<>();
        rem_list.addAll(markers.keySet());

        for (Integer user_id : ch.userlist) {
            rem_list.remove(user_id);

            User u = users.get(user_id);
            if (u.profile_image == null)
                continue;

            LatLng loc = new LatLng(u.l.getLatitude(), u.l.getLongitude());
            if (!markers.containsKey(user_id)) {
                Marker m = mMap.addMarker(new MarkerOptions()
                        .title(u.name)
                        .position(loc)
                        .icon(BitmapDescriptorFactory.fromBitmap(getCroppedBitmap(Bitmap.createScaledBitmap(u.profile_image, 128, 128, true)))));
                markers.put(user_id, m);
            }
            else {
                Marker m = markers.get(user_id);
                if (m != null)
                    m.setPosition(loc);
            }
        }

        for (Integer user_id : rem_list) {
            try {
                markers.get(user_id).remove();
            }
            catch (NullPointerException e) {}
            markers.remove(user_id);
        }

    }


    void joinPrompt() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Join channel");

        final EditText input = new EditText(this);
        input.setInputType(InputType.TYPE_CLASS_TEXT);
        builder.setView(input);

        builder.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                String channel = input.getText().toString();
                if (channel.length() > 0)
                    startJoinChannel(channel);
            }
        });
        builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.cancel();
            }
        });

        builder.show();
    }


    void startRegister() {
        try {
            Message msg = Message.obtain(null, AppService.REGISTER);
            msg.replyTo = mMessenger;
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void sendLocation(final Location loc) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("lon", loc.getLongitude());
            bundle.putSerializable("lat", loc.getLatitude());
            bundle.putSerializable("alt", loc.getAltitude());
            Message msg = Message.obtain(null, AppService.SENDLOCATION);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void changeChannel(final String ch) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("ch", ch);
            Message msg = Message.obtain(null, AppService.CHANGECHANNEL);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void startJoinChannel(final String ch) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("ch", ch);
            Message msg = Message.obtain(null, AppService.JOINCHANNEL);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void startLeaveChannel(final String ch) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("ch", ch);
            Message msg = Message.obtain(null, AppService.LEAVECHANNEL);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void startSendMessage(final String m) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("msg", m);
            Message msg = Message.obtain(null, AppService.SENDMESSAGE);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void setupFinish(String name, int age, boolean gender) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("setup_name", name);
            bundle.putSerializable("setup_age", age);
            bundle.putSerializable("setup_gender", gender);
            Message msg = Message.obtain(null, AppService.SETUPFINISH);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void settingsFinish(String name, int age, boolean gender, Bitmap profile_image) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("setup_name", name);
            bundle.putSerializable("setup_age", age);
            bundle.putSerializable("setup_gender", gender);
            bundle.putSerializable("profile_image", new BitmapDataObject(profile_image));
            Message msg = Message.obtain(null, AppService.SETTINGS);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void startAddMessages() {
        try {
            // return active channel
            Message msg = Message.obtain(null, AppService.ADDMESSAGES);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }

    void startSetProfileImage(Bitmap bm) {
        try {
            Bundle bundle = new Bundle();
            bundle.putSerializable("profile_image", new BitmapDataObject(bm));
            Message msg = Message.obtain(null, AppService.SETPROFILEIMAGE);
            msg.setData(bundle);
            mService.send(msg);
        } catch (RemoteException e) {

        } catch (NullPointerException e) {

        }
    }
}


