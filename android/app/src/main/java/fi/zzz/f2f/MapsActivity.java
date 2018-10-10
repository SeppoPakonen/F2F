package fi.zzz.f2f;

import android.Manifest;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.support.annotation.NonNull;
import android.support.design.widget.NavigationView;
import android.os.Bundle;
import com.google.android.gms.maps.CameraUpdateFactory;
import com.google.android.gms.maps.GoogleMap;
import com.google.android.gms.maps.OnMapReadyCallback;
import com.google.android.gms.maps.SupportMapFragment;
import com.google.android.gms.maps.model.BitmapDescriptor;
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
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Random;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class MapsActivity extends AppCompatActivity implements OnMapReadyCallback {

    public static MapsActivity last_maps;
    public static Bitmap profile_image;
    public static int setup_age = 0;
    public static boolean setup_gender = false;
    public static String setup_name = "Unnamed";

    private GoogleMap mMap;
    private DrawerLayout mDrawerLayout;

    private static final String TAG = "F2F";
    private static final int REQUEST_PERMISSION_LOCATION = 255; // int should be between 0 and 255

    public enum Preferencies {
        COMPLETED_ONBOARDING_PREF_NAME
    }

    // Client code
    private int user_id = -1;
    private byte[] pass;
    private boolean is_registered = false;

    private HashMap<String, Channel> channels;
    private HashMap<Integer, User> users;
    private HashMap<Long, Bitmap> image_cache;
    private HashSet<String> my_channels;
    private HashMap<Integer, Marker> markers;
    private Marker my_marker;
    private String user_name;
    private String addr;
    private String active_channel = "";
    private Socket sock;
    private int port = 17000;
    private int age = 0;
    private boolean gender = true;
    private boolean is_logged_in = false;
    private DataInputStream input;
    private DataOutputStream output;
    private Lock call_lock, lock;


    public MapsActivity() {
        last_maps = this;

        pass = new byte[8];
        call_lock = new ReentrantLock();
        lock = new ReentrantLock();
        channels = new HashMap<>();
        users = new HashMap<>();
        my_channels = new HashSet<>();
        markers = new HashMap<>();

    }



    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.activity_maps);

            loadThis();

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


            // First time user onboarding activity
            // https://developer.android.com/training/tv/playback/onboarding
            if (!is_registered) {
                // The user hasn't seen the OnboardingFragment yet, so show it
                startActivity(new Intent(this, OnboardingActivity.class));
            }
            else {
                startThread();
            }



            // Ask location permissions
            if (ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, REQUEST_PERMISSION_LOCATION);
            }
            else {
                StartLocationService();
            }


        }
        catch (java.lang.NullPointerException e) {
            Log.e(TAG, "System error");
            System.exit(1);
        }

    }

    public Channel getActiveChannel() {
        return channels.get(active_channel);
    }
    void startSettings() {
        startActivity(new Intent(this, SettingsActivity.class));
    }

    void startMessages() {
        startActivity(new Intent(this, MessagesActivity.class));
    }

    void startThread() {
        Thread thread = new Thread() {
            @Override
            public void run() {
                addr = "f2f.zzz.fi";
                port = 17000;
                connect();
                registerScript();
                loginScript();
                setup();
                handleConnection();
            }
        };
        thread.start();
    }

    void startJoinChannel(final String ch) {
        Thread thread = new Thread() {
            @Override
            public void run() {
                try {
                    joinChannel(ch);
                    active_channel = ch;
                    postRefreshGui();
                }
                catch (Exc e) {

                }
            }
        };
        thread.start();
    }

    void startLeaveChannel(final String ch) {
        Thread thread = new Thread() {
            @Override
            public void run() {
                try {
                    leaveChannel(ch);
                    if (active_channel.equals(ch)) {
                        if (my_channels.isEmpty())
                            setActiveChannel("");
                        else
                            setActiveChannel(my_channels.iterator().next());
                    }
                    postRefreshGui();
                }
                catch (Exc e) {

                }
            }
        };
        thread.start();
    }

    void startSendMessage(final String message) {
        Thread thread = new Thread() {
            @Override
            public void run() {
                try {
                    MapsActivity.last_maps.sendChannelmessage(active_channel, message);
                }
                catch (Exc e) {
                    Log.e(TAG, "Message sending failed");
                }
            }
        };
        thread.start();
    }

    void applySettings() {
        Thread thread = new Thread() {
            @Override
            public void run() {
                storeThis();
                setup();
            }
        };
        thread.start();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_PERMISSION_LOCATION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // We now have permission to use the location
                StartLocationService();
            }
        }
    }

    void StartLocationService() {
        LocationManager locationManager = (LocationManager) this.getSystemService(Context.LOCATION_SERVICE);

        // Define a listener that responds to location updates
        LocationListener locationListener = new LocationListener() {
            public void onLocationChanged(final Location location) {

                my_marker.setPosition(new LatLng(location.getLatitude(), location.getLongitude()));

                // Called when a new location is found by the network location provider.
                Thread thread = new Thread() {
                    @Override
                    public void run() {
                        try {
                            sendLocation(location);
                        }
                        catch (Exc e) {
                            Log.e(TAG, "Location changing failed: " + e.msg);
                        }
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


    class User {
        private int user_id = -1;
        private String name = "";
        private boolean is_updated = false;
        private Date last_update;

        private HashSet<String> channels;
        private Location l;
        private long profile_image_hash = 0;
        private int age = 0;
        private boolean gender = true;
        private Bitmap profile_image;

        User() {
            channels = new HashSet<>();
            l = new Location("");

            Calendar cal = Calendar.getInstance();
            cal.set(Calendar.YEAR, 1970);
            cal.set(Calendar.MONTH, 1);
            cal.set(Calendar.DAY_OF_MONTH, 1);
            last_update = cal.getTime();
        }
    }

    void writeBytes(DataOutputStream s, byte[] b) throws IOException {
        s.writeInt(b.length);
        s.write(b);
    }

    void writeString(DataOutputStream s, String str) throws IOException {
        writeBytes(s, str.getBytes());
    }

    void writeImage(DataOutputStream s, Bitmap bm) throws IOException {
        ByteArrayOutputStream baos= new ByteArrayOutputStream();
        bm.compress(Bitmap.CompressFormat.PNG,100, baos);
        byte [] b = baos.toByteArray();
        writeBytes(s, b);
    }

    byte[] readBytes(DataInputStream s) throws IOException {
        int len = s.readInt();
        if (len < 0 || len > 10000000) throw new IOException();
        byte[] b = new byte[len];
        s.read(b);
        return b;
    }

    String readString(DataInputStream s) throws IOException {
        return new String(readBytes(s));
    }

    Bitmap readImage(DataInputStream s) throws IOException {
        byte[] b = readBytes(s);
        return BitmapFactory.decodeByteArray(b, 0, b.length);
    }

    void storeThis() {
        String cl_file = getApplicationContext().getFilesDir() + "/Client.bin";
        try {
            FileOutputStream fout = new FileOutputStream(cl_file );
            DataOutputStream out = new DataOutputStream(fout);
            out.writeInt(user_id);
            writeBytes(out, pass);
            out.writeBoolean(is_registered);
            writeString(out, setup_name);
            out.writeInt(setup_age);
            out.writeBoolean(setup_gender);
            writeImage(out, profile_image);
            fout.close();
        }
        catch (IOException e) {
            Log.e(TAG, "Storing configuration failed");
        }
    }

    void loadThis() {
        String cl_file = getApplicationContext().getFilesDir() + "/Client.bin";
        try {
            FileInputStream fin = new FileInputStream(cl_file );
            DataInputStream in = new DataInputStream(fin);
            user_id = in.readInt();
            pass = readBytes(in);
            is_registered = in.readBoolean();
            setup_name = readString(in);
            setup_age = in.readInt();
            setup_gender = in.readBoolean();
            profile_image = readImage(in);
            fin.close();
        }
        catch (IOException e) {
            Log.e(TAG, "Storing configuration failed");
        }
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

    void postRefreshGui() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                refreshGui();
            }
        });
    }

    void postRefreshGuiChannel() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                refreshGuiChannel();
            }
        });
    }

    boolean connect() {
        if (sock == null || sock.isClosed()) {
            postSetTitle(getApplicationContext().getResources().getString(R.string.connecting));

            Log.i(TAG, "Connecting");
            is_logged_in = false;

            try {
                Log.i(TAG, "Connecting " + addr + ":" + Integer.toString(port));
                sock = new Socket(addr, port);

                input = new DataInputStream(this.sock.getInputStream());
                output = new DataOutputStream(this.sock.getOutputStream());

                ByteBuffer bb = ByteBuffer.allocate(4);
                bb.putInt(swap(-1));
                bb.position(0);
                while (bb.hasRemaining())
                    output.write(bb.get());
            }
            catch (UnknownHostException e1) {
                Log.w(TAG, "Couldn't resolve host");
                return false;
            }
            catch (IOException e) {
                Log.w(TAG, "Socket IO error");
                return false;
            }
        }
        return true;
    }

    boolean registerScript() {
        if (!is_registered) {
            postSetTitle(getApplicationContext().getResources().getString(R.string.registering));
            try {
                register();
                is_registered = true;
                storeThis();
            }
            catch (Exc e) {
                return false;
            }
            postSetTitle(getApplicationContext().getResources().getString(R.string.activity_maps));
        }
        return true;
    }

    boolean loginScript() {
        if (!is_logged_in) {
            postSetTitle(getApplicationContext().getResources().getString(R.string.logging_in));
            lock.lock();
            users.clear();
            channels.clear();
            lock.unlock();

            try {
                login();
                refreshChannellist();
                refreshUserlist();
                is_logged_in = true;
                postRefreshGui();
            }
            catch (Exc e) {
                return false;
            }
            finally {
                postSetTitle(getApplicationContext().getResources().getString(R.string.activity_maps));
            }
        }
        return true;
    }

    void setup() {
        postSetTitle(getApplicationContext().getResources().getString(R.string.setupping));
        setName(setup_name);
        setAge(setup_age);
        setGender(setup_gender);
        setImage(profile_image);
        postSetTitle(getApplicationContext().getResources().getString(R.string.activity_maps));
    }

    void setName(String s) {
        if (user_name != null && user_name.equals(s)) return;
        try {
            if (set("name", s.getBytes()))
                user_name = s;
        }
        catch (Exc e) {
            Log.e(TAG, "Changing name failed");
        }
    }

    void setAge(int i) {
        if (age == i) return;
        try {
            if (set("age", Integer.toString(i).getBytes()))
                age = i;
        }
        catch (Exc e) {
            Log.e(TAG, "Changing age failed");
        }
    }

    void setGender(boolean i) {
        if (gender == i) return;
        try {
            if (set("age", Integer.toString(i ? 1 : 0).getBytes()))
                gender = i;
        }
        catch (Exc e) {
            Log.e(TAG, "Changing age failed");
        }
    }

    void setImage(Bitmap i) {
        if (i == null) return;
        long hash = 0;
        try {
            byte[] hash_bytes = get("profile_image_hash");
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(hash_bytes));
            hash = swap(in.readInt()) & 0x00000000ffffffffL;
        }
        catch (IOException e) {
            Log.e(TAG, "Getting existing image hash failed");
        }
        catch (Exc e) {
            Log.e(TAG, "Getting existing image hash failed");
        }
        while (true) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            i.compress(Bitmap.CompressFormat.JPEG, 80, out);
            byte[] imgstr = out.toByteArray();
            if (imgstr.length > 100000) {
                int nw = i.getWidth() / 2;
                int nh = i.getHeight() / 2;
                i = Bitmap.createScaledBitmap(i, nw, nh, true);
            } else {
                if (hash != hash(imgstr)) {
                    try {
                        set("profile_image", imgstr);
                    }
                    catch (Exc e) {
                        Log.e(TAG, "Changing profile image failed");
                    }
                }
                break;
            }
        }
    }

    void storeImageCache(byte[] image_str) {
        long hash = hash(image_str);
        String img_file = getApplicationContext().getFilesDir() + "/" + Long.toString(hash) + ".bin";
        try {
            FileOutputStream fout = new FileOutputStream(img_file);
            fout.write(image_str, 0, image_str.length);
            fout.close();
        }
        catch (FileNotFoundException f) {
            Log.e(TAG, "File not found: " + img_file);
        }
        catch (IOException f) {
            Log.e(TAG, "IOException: " + img_file);
        }
    }

    boolean hasCachedImage(long hash) {
        String img_file = getApplicationContext().getFilesDir() + "/" + Long.toString(hash) + ".bin";
        File f = new File(img_file);
        return f.exists();
    }

    byte[] loadCachedImage(long hash) {
        String img_file = getApplicationContext().getFilesDir() + "/" + Long.toString(hash) + ".bin";
        try {
            long size = new File(img_file).length();
            byte[] data = new byte[(int)size];
            FileInputStream fin = new FileInputStream(img_file);
            fin.read(data);
            return data;
        }
        catch (FileNotFoundException f) {
            Log.e(TAG, "File not found: " + img_file);
        }
        catch (IOException f) {
            Log.e(TAG, "IOException: " + img_file);
        }
        return new byte[0];
    }

    void handleConnection() {
        postSetTitle(getApplicationContext().getResources().getString(R.string.activity_maps));
        Log.i(TAG, "Client connection running");
        int count = 0;

        while (!Thread.interrupted()) {
            connect();

            try {
                while (!Thread.interrupted() && sock.isConnected()) {
                    registerScript();
                    loginScript();

                    poll();
                    sleep(1000);
                    count++;
                }

                sock.close();
            }
            catch (Exc e) {
                Log.e(TAG, "Error: " + e);
            }
            catch (IOException e) {
                Log.e(TAG, "Error: IOException");
            }
            catch (NullPointerException e) {

            }

            is_logged_in = false;
            try {sock.close();} catch (IOException e) {} catch (NullPointerException e) {}
            sock = null;
        }

        Log.i(TAG, "Client connection stopped");
    }

    int swap(int i) {
        return ByteBuffer.allocate(4)
                .order(ByteOrder.BIG_ENDIAN).putInt(i)
                .order(ByteOrder.LITTLE_ENDIAN).getInt(0);
    }

    public static double swapDouble(double x) {
        return ByteBuffer.allocate(8)
                .order(ByteOrder.BIG_ENDIAN).putDouble(x)
                .order(ByteOrder.LITTLE_ENDIAN).getDouble(0);
    }

    private final static char[] hexArray = "0123456789ABCDEF".toCharArray();
    public static String bytesToHex(byte[] bytes) {
        char[] hexChars = new char[bytes.length * 2];
        for ( int j = 0; j < bytes.length; j++ ) {
            int v = bytes[j] & 0xFF;
            hexChars[j * 2] = hexArray[v >>> 4];
            hexChars[j * 2 + 1] = hexArray[v & 0x0F];
        }
        return new String(hexChars);
    }

    DataInputStream call(byte[] out_data) throws Exc {
        byte[] in_data;
        int r;

        if (sock == null)
            return new DataInputStream(new ByteArrayInputStream(new byte[0]));

        call_lock.lock();
        try {
            sock.setSoTimeout(30000);

            output.writeInt(swap(out_data.length));
            output.write(out_data);
            //Log.i(TAG, bytesToHex(out_data));

            int in_size = swap(input.readInt());
            if (in_size <= 0 || in_size > 10000000)
                throw new SocketException();
            in_data = new byte[in_size];
            for (int i = 0; i < 100 && input.available() < in_size; i++) sleep(10);
            r = input.read(in_data);
            if (r != in_size)
                throw new IOException();
            //Log.i(TAG, bytesToHex(in_data));
        }
        catch (SocketException e) {
            throw new Exc("Call: Socket exception");
        }
        catch (EOFException e) {
            throw new Exc("Call: EOFException ");
        }
        catch (IOException e) {
            throw new Exc("Call: IOException");
        }
        finally {
            call_lock.unlock();
        }
        return new DataInputStream(new ByteArrayInputStream(in_data));
    }

    void sleep(int ms) {
        try {
            Thread.sleep(ms);
        }
        catch (InterruptedException e) {}
    }

    long hash(byte[] s) {
        return memhash(s, s.length);
    }

    long memhash(byte[] ptr, int count) {
        int hash = 1234567890;
        for (int i = 0; i < count; i++)
            hash = ((hash << 5) - hash) ^ ptr[i];
        return hash & 0x00000000ffffffffL;
    }

    void writeInt(ByteArrayOutputStream out, int i) {
        out.write((i >> 24) & 0xFF);
        out.write((i >> 16) & 0xFF);
        out.write((i >> 8) & 0xFF);
        out.write(i & 0xFF);
    }

    void register() throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(10));

            DataInputStream in = call(dout.toByteArray());

            user_id = swap(in.readInt());
            byte[] pass_bytes = new byte[8];
            in.read(pass_bytes);
            pass = pass_bytes;

            Log.i(TAG, "Client " + Integer.toString(user_id) + " registered (pass " + new String(pass) + ")");
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    void login() throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(20));
            out.writeInt(swap(user_id));
            out.write(pass);

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());

            int name_len = swap(in.readInt());
            byte[] name_bytes = new byte[name_len];
            in.read(name_bytes);
            user_name = new String(name_bytes);
            age = swap(in.readInt());
            gender = swap(in.readInt()) != 0;
            Log.i(TAG, "Client " + Integer.toString(user_id) + " logged in (" + Integer.toString(user_id) + "," + new String(pass) + ") name: " + user_name);
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }
    boolean set(String key, byte[] value) throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(30));
            out.writeInt(swap(key.length()));
            out.write(key.getBytes());
            out.writeInt(swap(value.length));
            out.write(value);

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());
            if (ret == 1) {
                Log.e(TAG, "Client set " + key + " failed");
                return false;
            }
            Log.i(TAG, "Client set " + key);
            return true;
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    byte[] get(String key) throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(40));
            out.writeInt(swap(key.length()));
            out.write(key.getBytes(StandardCharsets.ISO_8859_1));

            DataInputStream in = call(dout.toByteArray());

            int value_len = swap(in.readInt());
            if (value_len < 0 || value_len > 10000000) {
                Log.e(TAG, "Get; Invalid length");
                return new byte[0];
            }
            byte[] value_bytes = new byte[value_len];
            in.read(value_bytes);

            Log.i(TAG, "Client get " + key);
            return value_bytes;
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    void joinChannel(String channel) throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(50));
            out.writeInt(swap(channel.length()));
            out.writeBytes(channel);

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());
            if (ret == 1) {
                Log.w(TAG, "Client was already joined to channel " + channel);
                return;
            }
            else if (ret != 0) throw new Exc("Joining channel failed)");

            my_channels.add(channel);
            channels.put(channel, new Channel());
            postRefreshGui();

            Log.i(TAG, "Client joined channel " + channel);
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    void leaveChannel(String channel) throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(60));
            out.writeInt(swap(channel.length()));
            out.writeBytes(channel);

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());
            if (ret != 0) throw new Exc("Leaving channel failed)");

            my_channels.remove(channel);
            postRefreshGui();

            Log.i(TAG, "Client left channel " + channel);
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    void message(int recv_user_id, String msg) throws Exc {
        if (recv_user_id < 0) return;
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(70));
            out.writeInt(swap(msg.length()));
            out.writeBytes(msg);

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());
            if (ret != 0) throw new Exc("Message sending failed)");

            Log.i(TAG, "Message to " + Integer.toString(recv_user_id) + " sent: " + msg);
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    class UserJoined {
        private int user_id;
        private String channel;
    }

    int find(byte[] b, int chr) {
        for (int i = 0; i < b.length; i++) {
            if (b[i] == chr)
                return i;
        }
        return -1;
    }

    String midString(byte[] b, int begin, int length) {
        return new String(mid(b, begin, length));
    }

    byte[] mid(byte[] b, int begin, int length) {
        byte[] out = new byte[length];
        for(int i = 0; i < length; i++)
            out[i] = b[begin + i];
        return out;
    }

    void poll() throws Exc {
        List<UserJoined> join_list = new ArrayList<>();

        lock.lock();

        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(80));

            DataInputStream in = call(dout.toByteArray());

            int count = swap(in.readInt());
            if (count < 0 || count >= 10000) {lock.unlock(); throw new Exc("Polling failed");}
            for(int i = 0; i < count; i++) {
                int sender_id = swap(in.readInt());
                int message_len = swap(in.readInt());
                if (message_len < 0 ||message_len > 1000000) {
                    Log.e(TAG, "Invalid message");
                    break;
                }
                byte[] message_bytes = new byte[message_len];
                in.read(message_bytes);

                int j = find(message_bytes, ' ');
                if (j == -1) continue;
                String key = midString(message_bytes, 0, j);
                message_bytes = mid(message_bytes, j+1, message_bytes.length - j-1);


                Log.i(TAG, "Poll " + Integer.toString(i) + ": " + key);

                if (key.equals("msg")) {
                    if (!users.containsKey(sender_id)) continue;
                    String message = new String(message_bytes);
                    String ch_name = "user" + Integer.toString(sender_id);
                    User u = users.get(sender_id);
                    my_channels.add(ch_name);
                    if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                    Channel ch = channels.get(ch_name);
                    ch.userlist.add(sender_id);
                    ch.post(ch_name, sender_id, u.name, message, false, u.profile_image);
                    postRefreshGui();
                }
                else if (key.equals("chmsg")) {
                    if (!users.containsKey(sender_id)) continue;
                    String message = new String(message_bytes);
                    User u = users.get(sender_id);
                    j = message.indexOf(" ");
                    String ch_name = message.substring(0, j);
                    message = message.substring(j + 1);
                    if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                    Channel ch = channels.get(ch_name);
                    ch.post(ch_name, sender_id, u.name, message, false, u.profile_image);
                    postRefreshGui();
                }
                else if (key.equals("join")) {
                    String message = new String(message_bytes);
                    String[] args = message.split(" ");
                    if (args.length != 2) continue;
                    int user_id = Integer.parseInt(args[0]);
                    String ch_name = args[1];
                    if (!users.containsKey(user_id)) users.put(user_id, new User());
                    User u = users.get(user_id);
                    u.user_id = user_id;
                    u.channels.add(ch_name);
                    if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                    Channel ch = channels.get(ch_name);
                    ch.userlist.add(user_id);
                    UserJoined uj = new UserJoined();
                    uj.user_id = user_id;
                    uj.channel = ch_name;
                    join_list.add(uj);
                }
                else if (key.equals("leave")) {
                    String message = new String(message_bytes);
                    String[] args = message.split(" ");
                    if (args.length != 2) continue;
                    int user_id = Integer.parseInt(args[0]);
                    String ch_name = args[1];
                    if (!users.containsKey(user_id)) continue;
                    User u = users.get(user_id);
                    u.channels.remove(ch_name);
                    if (!channels.containsKey(ch_name)) continue;
                    Channel ch = channels.get(ch_name);
                    ch.userlist.remove(user_id);
                    ch.post(ch_name, -1, "Server", "User " + u.name + " left channel " + ch_name, false, u.profile_image);
                    if (u.channels.isEmpty())
                        users.remove(user_id);
                    postRefreshGui();
                }
                else if (key.equals("name")) {
                    String message = new String(message_bytes);
                    String[] args = message.split(" ");
                    if (args.length != 2) continue;
                    int user_id = Integer.parseInt(args[0]);
                    String user_name = args[1];
                    if (!users.containsKey(user_id)) continue;
                    User u = users.get(user_id);
                    u.name = user_name;
                    for (String ch_name : u.channels) {
                        if (!my_channels.contains(ch_name)) continue;
                        if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                        Channel ch = channels.get(ch_name);
                        ch.post(ch_name, -1, "Server", "User " + u.name + " changed name to " + user_name, false, u.profile_image);
                    }
                    postRefreshGui();
                }
                else if (key.equals("loc")) {
                    String message = new String(message_bytes);
                    String[] args = message.split(" ");
                    if (args.length != 4) continue;
                    final int user_id = Integer.parseInt(args[0]);
                    final double lon = Double.parseDouble(args[1]);
                    final double lat = Double.parseDouble(args[2]);
                    final double elev = Double.parseDouble(args[3]);
                    if (!users.containsKey(user_id)) continue;
                    User u = users.get(user_id);
                    u.last_update = Calendar.getInstance().getTime();
                    u.l.setLongitude(lon);
                    u.l.setLatitude(lat);
                    u.l.setAltitude(elev);
                    for (String ch_name : u.channels) {
                        if (!my_channels.contains(ch_name)) continue;
                        if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                        Channel ch = channels.get(ch_name);
                        //ch.post(ch_name, -1, "Server", "User " + u.name + " updated location to " + Double.toString(lon) + "," + Double.toString(lat), false, u.profile_image);
                    }
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            Marker m = markers.get(user_id);
                            if (m != null) {
                                m.setPosition(new LatLng(lat, lon));
                            }
                        }
                    });
                    postRefreshGui();
                }
                else if (key.equals("profile")) {
                    j = find(message_bytes, ' ');
                    if (j == -1) continue;
                    String user_id_str = midString(message_bytes, 0, j);
                    final int user_id = Integer.parseInt(user_id_str);
                    byte[] img = mid(message_bytes, j+1, message_bytes.length-j-1);
                    storeImageCache(img);
                    if (users.containsKey(user_id)) {
                        final User u = users.get(user_id);
                        Bitmap bm = BitmapFactory.decodeByteArray(img, 0, img.length);
                        if (bm != null)
                            u.profile_image = bm;
                        u.profile_image_hash = hash(img);
                        for (String ch_name : u.channels) {
                            if (!my_channels.contains(ch_name)) continue;
                            if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                            Channel ch = channels.get(ch_name);
                            //ch.post(ch_name, -1, "Server", "User " + u.name + " updated profile image", u.profile_image);
                        }
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
                                            m.setIcon(BitmapDescriptorFactory.fromBitmap(getCroppedBitmap(Bitmap.createScaledBitmap(randomUserImage(), 128, 128, true))));
                                        } catch (IllegalArgumentException e) {
                                            Log.e(TAG, "IllegalArgumentException");
                                        }
                                    }
                                }
                            }
                        });
                    }
                    postRefreshGui();
                }
            }
        }
        catch (IOException e) {
            Log.e(TAG, "Poll: IOEXception");
        }
        catch (NullPointerException e) {
            Log.e(TAG, "Poll: NullPointerException");
        }
        finally {
            lock.unlock();
        }

        if (!join_list.isEmpty()) {

            for (UserJoined uj : join_list) {

                byte[] who = get("who " + Integer.toString(uj.user_id));
                DataInputStream in = new DataInputStream(new ByteArrayInputStream(who));
                who(in);

                User u = users.get(uj.user_id);
                if (u == null) {
                    Log.e(TAG,"User still null");
                    continue;
                }
                refreshUserImage(u);

                String ch_name = uj.channel;
                if (!channels.containsKey(ch_name)) continue;
                Channel ch = channels.get(ch_name);
                ch.userlist.add(uj.user_id);
                ch.post(ch_name, -1, "Server", "User " + u.name + " joined channel " + uj.channel, false, u.profile_image);
            }

            postRefreshGui();
        }
    }

    void sendLocation(Location l) throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(90));

            out.writeDouble(swapDouble(l.getLatitude()));
            out.writeDouble(swapDouble(l.getLongitude()));
            out.writeDouble(swapDouble(l.getAltitude()));

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());
            if (ret != 0) throw new Exc("Updating location failed)");

            Log.i(TAG, "Client updated location ");
        }
        catch (IOException e) {
            throw new Exc("sendLocation: IOException");
        }
        catch (NullPointerException e) {
            throw new Exc("sendLocation: not connected yet");
        }
    }

    void sendChannelmessage(String channel, String msg) throws Exc {
        try {
            ByteArrayOutputStream dout = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(dout);

            out.writeInt(swap(100));

            out.writeInt(swap(channel.length()));
            out.writeBytes(channel);
            out.writeInt(swap(msg.length()));
            out.writeBytes(msg);

            DataInputStream in = call(dout.toByteArray());

            int ret = swap(in.readInt());
            if (ret != 0) throw new Exc("Message sending failed)");

            Log.i(TAG, "Client sent to channel " + channel + " message " + msg);
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    void refreshChannellist() throws Exc {
        try {
            byte[] channellist_str = get("channellist");
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(channellist_str));

            HashSet<String> ch_rem = new HashSet<>();
            ch_rem.addAll(my_channels);

            int ch_count = swap(in.readInt());
            for (int i = 0; i < ch_count; i++) {
                int name_len = swap(in.readInt());
                if (name_len <= 0) continue;
                byte[] name_bytes = new byte[name_len];
                in.read(name_bytes);
                String name = new String(name_bytes);

                ch_rem.remove(name);

                my_channels.add(name);
                if (!channels.containsKey(name)) channels.put(name, new Channel());
            }

            for (String ch : ch_rem)
                my_channels.remove(ch);

            if (!my_channels.isEmpty() && active_channel.isEmpty()) {
                setActiveChannel(my_channels.iterator().next());
                postRefreshGuiChannel();
            }

            Log.i(TAG, "Client updated channel-list");
        }
        catch (IOException e) {
            throw new Exc("Register: IOException");
        }
    }

    boolean who(DataInputStream in) {
        try {
            boolean success = true;
            int user_id = swap(in.readInt());
            int name_len = swap(in.readInt());
            if (name_len < 0 || name_len > 200)
                return false;
            byte[] name_bytes = new byte[name_len];
            in.read(name_bytes);
            String name = new String(name_bytes);

            if (!users.containsKey(user_id)) users.put(user_id, new User());
            User u = users.get(user_id);
            u.user_id = user_id;
            u.name = name;
            u.age = swap(in.readInt());
            u.gender = swap(in.readInt()) != 0;

            u.profile_image_hash = swap(in.readInt()) & 0x00000000ffffffffL;

            u.l.setLongitude(swapDouble(in.readDouble()));
            u.l.setLatitude(swapDouble(in.readDouble()));
            u.l.setAltitude(swapDouble(in.readDouble()));

            int channel_count = swap(in.readInt());
            if (channel_count < 0 || channel_count > 200)
                return false;
            for (int j = 0; j < channel_count; j++) {
                int ch_name_len = swap(in.readInt());
                if (ch_name_len < 0 ||ch_name_len >= 1000)
                    return false;
                byte[] ch_name_bytes = new byte[ch_name_len];
                in.read(ch_name_bytes);
                String ch_name = new String(ch_name_bytes);
                u.channels.add(ch_name);
                if (!channels.containsKey(ch_name)) channels.put(ch_name, new Channel());
                Channel ch = channels.get(ch_name);
                ch.userlist.add(user_id);
            }

            return true;
        }
        catch (IOException e) {
            return false;
        }
    }

    void refreshUserlist() {
        try {
            byte[] userlist_str = get("userlist");
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(userlist_str));

            int user_count = swap(in.readInt());
            boolean fail = false;
            for(int i = 0; i < user_count && !fail; i++) {
                fail = fail || !who(in);
            }
            if (fail) throw new Exc("Getting userlist failed");

            for (User u : users.values())
                refreshUserImage(u);

            Log.i(TAG, "Client updated userlist");
        }
        catch (Exc e) {
            Log.e(TAG, "Refreshing userlist failed: " + e.msg);
        }
        catch (IOException e) {
            Log.e(TAG, "Refreshing userlist failed: IOException");
        }
    }

    public static Bitmap randomUserImage() {
        Random rnd = new Random();
        Bitmap bm = Bitmap.createBitmap(128, 128, Bitmap.Config.ARGB_8888);
        bm.eraseColor(Color.argb(255, rnd.nextInt(256), rnd.nextInt(256), rnd.nextInt(256)));
        return bm;
    }

    void refreshUserImage(User u) {
        try {
            byte[] image_str;

            if (!hasCachedImage(u.profile_image_hash)) {
                // Fetch image
                image_str = get("image " + Long.toString(u.profile_image_hash));

                // Store to hard drive
                storeImageCache(image_str);

            } else {
                image_str = loadCachedImage(u.profile_image_hash);
            }

            // Load to memory
            Bitmap bm = BitmapFactory.decodeByteArray(image_str, 0, image_str.length);
            if (bm != null)
                u.profile_image = bm;
            else {
                u.profile_image = randomUserImage();
            }
        }
        catch (Exc e) {
            Log.e(TAG, "User image refresh failed: " + u.name);
        }
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


    void changeChannel(String ch) {
        Log.i(TAG, "Change channel to " + ch);

        setActiveChannel(ch);
        postRefreshGui();
    }

    void refreshGui() {
        lock.lock();


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
                    if (ch != active_channel)
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



        if (!channels.containsKey(active_channel)) {lock.unlock(); return;}

        Channel ch = channels.get(active_channel);
        if (ch == null) {lock.unlock(); return;}

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

        lock.unlock();
    }

    void refreshGuiChannel() {

    }

    void setActiveChannel(String s) {
        active_channel = s;
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

    boolean isActiveChannel(String ch) {return active_channel.equals(ch);}

    String getActiveChannelString() {return active_channel;}

}




class ChannelMessage {
    int sender_id = -1;
    String message = "", sender_name = "";
    Date received;
    boolean belongs_to_user = false;
    Bitmap icon;
}

class Channel {
    List<ChannelMessage> messages;
    HashSet<Integer> userlist;
    int unread = 0;

    Channel() {
        messages = new ArrayList<>();
        userlist = new HashSet<>();
    }
    void post(String ch_name, int user_id, String user_name, String msg, boolean belongs_to_user, Bitmap icon) {
        ChannelMessage m = new ChannelMessage();
        m.received = Calendar.getInstance().getTime();
        m.message = msg;
        m.sender_id = user_id;
        m.sender_name = user_name;
        m.belongs_to_user = belongs_to_user;
        m.icon = icon;
        messages.add(m);
        Log.i("Channel", Integer.toString(user_id) + ", " + user_name + " sent to channel " + ch_name + " message " + msg);

        if (MapsActivity.last_maps.isActiveChannel(ch_name)) {
            if (MessagesActivity.last_act != null)
                MessagesActivity.last_act.gotMessage(user_name, msg, icon);
        }
    }
}

class Exc extends Throwable {
    String msg;
    Exc(String s) {msg = s;}
}

