package fi.zzz.f2f;

import android.content.ContextWrapper;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.Image;
import android.preference.PreferenceManager;
import android.support.design.widget.NavigationView;
import android.support.v4.app.FragmentActivity;
import android.os.Bundle;

import com.google.android.gms.maps.CameraUpdateFactory;
import com.google.android.gms.maps.GoogleMap;
import com.google.android.gms.maps.OnMapReadyCallback;
import com.google.android.gms.maps.SupportMapFragment;
import com.google.android.gms.maps.model.LatLng;
import com.google.android.gms.maps.model.MarkerOptions;

import android.support.v4.view.GravityCompat;
import android.support.v4.widget.DrawerLayout;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.MenuItem;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.Socket;
import java.net.SocketException;
import java.net.URLEncoder;
import java.net.UnknownHostException;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.Base64;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.locks.Lock;

import static java.lang.System.out;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;
import javax.crypto.IllegalBlockSizeException;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.security.MessageDigest;
import java.security.SecureRandom;
import static java.nio.charset.StandardCharsets.*;

class OpenSslAes {

    /** OpenSSL's magic initial bytes. */
    private static final String SALTED_STR = "Salted__";
    private static final byte[] SALTED_MAGIC = SALTED_STR.getBytes();



    /**
     *
     * @param password  The password / key to encrypt with.
     * @param clearText The data to encrypt
     * @return  A base64 encoded string containing the encrypted data.
     */
    static String encrypt(String password, String clearText) {
        try {
            final byte[] pass = password.getBytes();
            final byte[] salt = (new SecureRandom()).generateSeed(8);
            final byte[] inBytes = clearText.getBytes();

            final byte[] passAndSalt = array_concat(pass, salt);
            byte[] hash = new byte[0];
            byte[] keyAndIv = new byte[0];
            for (int i = 0; i < 3 && keyAndIv.length < 48; i++) {
                final byte[] hashData = array_concat(hash, passAndSalt);
                final MessageDigest md = MessageDigest.getInstance("MD5");
                hash = md.digest(hashData);
                keyAndIv = array_concat(keyAndIv, hash);
            }

            final byte[] keyValue = Arrays.copyOfRange(keyAndIv, 0, 32);
            final byte[] iv = Arrays.copyOfRange(keyAndIv, 32, 48);
            final SecretKeySpec key = new SecretKeySpec(keyValue, "AES");

            final Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
            cipher.init(Cipher.ENCRYPT_MODE, key, new IvParameterSpec(iv));
            byte[] data = cipher.doFinal(inBytes);
            data = array_concat(array_concat(SALTED_MAGIC, salt), data);
            return Base64.getEncoder().encodeToString(data);
        }
        catch (NoSuchAlgorithmException e) {}
        catch (InvalidAlgorithmParameterException e) {}
        catch (BadPaddingException e) {}
        catch (NoSuchPaddingException e) {}
        catch (InvalidKeyException e) {}
        catch (IllegalBlockSizeException e) {}
        return "";
    }

    /**
     * @see http://stackoverflow.com/questions/32508961/java-equivalent-of-an-openssl-aes-cbc-encryption  for what looks like a useful answer.  The not-yet-commons-ssl also has an implementation
     * @param password
     * @param source The encrypted data
     * @return
     */
    static String decrypt(String password, String source) {
        try {
            final byte[] pass = password.getBytes(US_ASCII);

            final byte[] inBytes = Base64.getDecoder().decode(source);

            final byte[] shouldBeMagic = Arrays.copyOfRange(inBytes, 0, SALTED_MAGIC.length);
            if (!Arrays.equals(shouldBeMagic, SALTED_MAGIC)) {
                throw new IllegalArgumentException("Initial bytes from input do not match OpenSSL SALTED_MAGIC salt value.");
            }

            final byte[] salt = Arrays.copyOfRange(inBytes, SALTED_MAGIC.length, SALTED_MAGIC.length + 8);

            final byte[] passAndSalt = array_concat(pass, salt);

            byte[] hash = new byte[0];
            byte[] keyAndIv = new byte[0];
            for (int i = 0; i < 3 && keyAndIv.length < 48; i++) {
                final byte[] hashData = array_concat(hash, passAndSalt);
                final MessageDigest md = MessageDigest.getInstance("MD5");
                hash = md.digest(hashData);
                keyAndIv = array_concat(keyAndIv, hash);
            }

            final byte[] keyValue = Arrays.copyOfRange(keyAndIv, 0, 32);
            final SecretKeySpec key = new SecretKeySpec(keyValue, "AES");

            final byte[] iv = Arrays.copyOfRange(keyAndIv, 32, 48);

            final Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
            cipher.init(Cipher.DECRYPT_MODE, key, new IvParameterSpec(iv));
            final byte[] clear = cipher.doFinal(inBytes, 16, inBytes.length - 16);
            return new String(clear, UTF_8);
        }
        catch (NoSuchAlgorithmException e) {}
        catch (InvalidAlgorithmParameterException e) {}
        catch (BadPaddingException e) {}
        catch (NoSuchPaddingException e) {}
        catch (InvalidKeyException e) {}
        catch (IllegalBlockSizeException e) {}
        return "";
    }


    private static byte[] array_concat(final byte[] a, final byte[] b) {
        final byte[] c = new byte[a.length + b.length];
        System.arraycopy(a, 0, c, 0, a.length);
        System.arraycopy(b, 0, c, a.length, b.length);
        return c;
    }
}


public class MapsActivity extends AppCompatActivity implements OnMapReadyCallback {

    private GoogleMap mMap;
    private DrawerLayout mDrawerLayout;

    private static final String TAG = "F2F";

    public enum Preferencies {
        COMPLETED_ONBOARDING_PREF_NAME
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        try {
            super.onCreate(savedInstanceState);
            setContentView(R.layout.activity_maps);

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
                        public boolean onNavigationItemSelected(MenuItem menuItem) {
                            // set item as selected to persist highlight
                            menuItem.setChecked(true);
                            // close drawer when item is tapped
                            mDrawerLayout.closeDrawers();
                            // Add code here to update the UI based on the item selected
                            // For example, swap UI fragments here
                            return true;
                        }
                    });


            // Set the toolbar as the action bar
            Toolbar toolbar = findViewById(R.id.toolbar);
            setSupportActionBar(toolbar);
            ActionBar actionbar = getSupportActionBar();
            actionbar.setDisplayHomeAsUpEnabled(true);
            actionbar.setHomeAsUpIndicator(R.drawable.ic_menu);


            // First time user onboarding activity
            // https://developer.android.com/training/tv/playback/onboarding
            /*SharedPreferences sharedPreferences =
                    PreferenceManager.getDefaultSharedPreferences(this);
            // Check if we need to display our OnboardingFragment
            if (!sharedPreferences.getBoolean(
                    Preferencies.COMPLETED_ONBOARDING_PREF_NAME, false)) {
                // The user hasn't seen the OnboardingFragment yet, so show it
                startActivity(new Intent(this, OnboardingActivity.class));
            }*/

        }
        catch (java.lang.NullPointerException e) {
            Log.e(TAG, "System error");
            System.exit(1);
        }
    }


    /**
     * Manipulates the map once available.
     * This callback is triggered when the map is ready to be used.
     * This is where we can add markers or lines, add listeners or move the camera. In this case,
     * we just add a marker near Sydney, Australia.
     * If Google Play services is not installed on the device, the user will be prompted to install
     * it inside the SupportMapFragment. This method will only be triggered once the user has
     * installed Google Play services and returned to the app.
     */
    @Override
    public void onMapReady(GoogleMap googleMap) {
        mMap = googleMap;

        // Add a marker in Sydney and move the camera
        LatLng oulu_uni = new LatLng(65.05919, 25.46748);
        mMap.addMarker(new MarkerOptions().position(oulu_uni).title("Marker in Oulu University"));
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

    }

    class Channel {

    }

    class Exc extends Throwable {
        String msg;
        Exc(String s) {msg = s;}
    }

    // Client code
    private int user_id = -1;
    private String pass;
    private boolean is_registered = false;

    private Map<String, Channel> channels;
    private Map<Integer, User> users;
    private Map<Long, Bitmap> image_cache;
    private Set<String> my_channels;
    private String user_name;
    private String addr;
    private Socket sock;
    private int port = 17000;
    private int age = 0;
    private boolean gender = false;
    private boolean is_logged_in = false;
    private DataInputStream input;
    private DataOutputStream output;
    private Lock call_lock, lock;

    void StoreThis() {

    }

    void LoadThis() {

    }

    void RefreshGui() {

    }

    void PostRefreshGui() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RefreshGui();
            }
        });
    }

    boolean Connect() {
        if (sock == null || sock.isClosed()) {
            is_logged_in = false;

            try {
                sock = new Socket(addr, port);

                input = new DataInputStream(this.sock.getInputStream());
                output = new DataOutputStream (this.sock.getOutputStream());

                output.writeInt(-1);
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

    boolean LoginScript() throws Exc {
        if (!is_registered) {
            try {
                Register();
                is_registered = true;
                StoreThis();
            }
            catch (Exc e) {
                return false;
            }
        }
        return true;
    }

    boolean RegisterScript() throws Exc {
        if (!is_logged_in) {
            lock.lock();
            users.clear();
            channels.clear();
            lock.unlock();

            try {
                Login();
                RefreshChannellist();
                RefreshUserlist();
                RefreshLocation();
                is_logged_in = true;
                PostRefreshGui();
            }
            catch (Exc e) {
                return false;
            }
        }
        return true;
    }

    void SetName(String s) {
        if (user_name == s) return;
        try {
            if (Set("name", s))
                user_name = s;
        }
        catch (Exc e) {
            Log.e(TAG, "Changing name failed");
        }
    }

    void SetAge(int i) {
        if (age == i) return;
        try {
            if (Set("age", Integer.toString(i)))
                age = i;
        }
        catch (Exc e) {
            Log.e(TAG, "Changing age failed");
        }
    }

    void SetGender(boolean i) {
        if (gender == i) return;
        try {
            if (Set("age", Integer.toString(i ? 1 : 0)))
                gender = i;
        }
        catch (Exc e) {
            Log.e(TAG, "Changing age failed");
        }
    }

    void SetImage(Bitmap i) {
        long hash = 0;
        try {
            String hash_str = Get("profile_image_hash");
            hash = Long.valueOf(hash_str);
        }
        catch (Exc e) {
            Log.e(TAG, "Getting existing image hash failed");
        }
        while (true) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            i.compress(Bitmap.CompressFormat.JPEG, 80, out);
            String imgstr = out.toByteArray().toString();
            if (imgstr.length() > 100000) {
                int nw = i.getWidth() / 2;
                int nh = i.getHeight() / 2;
                i = Bitmap.createScaledBitmap(i, nw, nh, true);
            } else {
                if (hash != Hash(imgstr)) {
                    try {
                        Set("profile_image", imgstr);
                    }
                    catch (Exc e) {
                        Log.e(TAG, "Changing profile image failed");
                    }
                }
                break;
            }
        }
    }

    void StoreImageCache(String image_str) {
        long hash = Hash(image_str);
        String img_file = getApplicationContext().getFilesDir() + "/" + Long.toString(hash) + ".bin";
        try {
            FileOutputStream fout = new FileOutputStream(img_file);
            fout.write(image_str.getBytes(), 0, image_str.length());
            fout.close();
        }
        catch (FileNotFoundException f) {
            Log.e(TAG, "File not found: " + img_file);
        }
        catch (IOException f) {
            Log.e(TAG, "IOException: " + img_file);
        }
    }

    boolean HasCachedImage(long hash) {
        String img_file = getApplicationContext().getFilesDir() + "/" + Long.toString(hash) + ".bin";
        File f = new File(img_file);
        return f.exists();
    }

    String LoadCachedImage(long hash) {
        String img_file = getApplicationContext().getFilesDir() + "/" + Long.toString(hash) + ".bin";
        //Bitmap bm = BitmapFactory.decodeFile(img_file);
        try {
            FileInputStream fin = new FileInputStream(img_file);
            return fin.toString();
        }
        catch (FileNotFoundException f) {
            Log.e(TAG, "File not found: " + img_file);
        }
        catch (IOException f) {
            Log.e(TAG, "IOException: " + img_file);
        }
        return "";
    }

    void HandleConnection() {
        Log.i(TAG, "Client connection running");
        int count = 0;

        while (!Thread.interrupted()) {
            Connect();

            try {
                while (Thread.interrupted() && sock.isConnected()) {
                    RegisterScript();
                    LoginScript();

                    Poll();
                    Sleep(1000);
                    count++;
                }

                sock.close();
            }
            catch (Exc e) {
                Log.e(TAG, "Error: " + e);
            }
            catch (IOException e) {}

            is_logged_in = false;
        }

        Log.i(TAG, "Client connection stopped");
    }

    ByteArrayInputStream Call(OutputStream out) throws Exc {
        String out_data = OpenSslAes.encrypt("passw0rdpassw0rd", out.toString());
        String in_data;

        call_lock.lock();
        try {
            sock.setSoTimeout(30000);
            output.writeInt(out_data.length());
            output.writeBytes(out_data);

            int in_size = input.readInt();
            byte[] in_buf = new byte[in_size];
            input.read(in_buf, 0, in_size);
            in_data = in_buf.toString();
        }
        catch (SocketException e) {call_lock.unlock(); throw new Exc("Call: Socket exception");}
        catch (IOException e) {call_lock.unlock(); throw new Exc("Call: IOException");}
        call_lock.unlock();

        String dec_data = OpenSslAes.decrypt("passw0rdpassw0rd", in_data);
        return new ByteArrayInputStream(dec_data.getBytes());
    }

    void Sleep(int ms) {
        try {
            Thread.sleep(ms);
        }
        catch (InterruptedException e) {}
    }

    long Hash(String s) {
        return memhash(s.getBytes(), s.length());
    }

    long memhash(byte[] ptr, int count) {
        int hash = 1234567890;
        for (int i = 0; i < count; i++)
            hash = ((hash << 5) - hash) ^ ptr[i];
        return hash & 0x00000000ffffffffL;
    }

    void Register() throws Exc {

    }

    void Login() throws Exc {

    }

    void RefreshChannellist() {

    }

    void RefreshUserlist() {

    }

    void RefreshLocation() {

    }

    boolean Set(String key, String value) throws Exc {

        return true;
    }

    String Get(String key) throws Exc {

        return "";
    }

    void Poll() {

    }
}
