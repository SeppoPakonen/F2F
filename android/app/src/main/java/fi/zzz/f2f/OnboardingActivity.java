package fi.zzz.f2f;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.support.v7.widget.AppCompatImageView;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import java.io.FileNotFoundException;
import java.io.InputStream;

public class OnboardingActivity extends Activity {

    public static final int PICK_IMAGE = 1;

    public OnboardingActivity() {
        MapsActivity.profile_image = MapsActivity.randomUserImage();

    }

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.onboarding1);

        Button next1 = (Button)findViewById(R.id.next1);
        next1.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                SetPage2();
            }
        });


    }

    void SetPage2() {
        setContentView(R.layout.onboarding2);

        Button select_image = findViewById(R.id.select_image1);
        select_image.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent getIntent = new Intent(Intent.ACTION_GET_CONTENT);
                getIntent.setType("image/*");

                Intent pickIntent = new Intent(Intent.ACTION_PICK, android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI);
                pickIntent.setType("image/*");

                Intent chooserIntent = Intent.createChooser(getIntent, "Select Image");
                chooserIntent.putExtra(Intent.EXTRA_INITIAL_INTENTS, new Intent[] {pickIntent});

                startActivityForResult(chooserIntent, PICK_IMAGE);
            }
        });

        Button next2 = (Button)findViewById(R.id.next2);
        next2.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                SetPage3();
            }
        });

        AppCompatImageView view = findViewById(R.id.profile_image_view);
        view.setImageBitmap(MapsActivity.profile_image);

    }

    void SetPage3() {
        setContentView(R.layout.onboarding3);

        Spinner dropdown = findViewById(R.id.edit_gender);
        String[] items = new String[]{"Female", "Male"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, items);
        dropdown.setAdapter(adapter);

        Button next2 = (Button)findViewById(R.id.next3);
        next2.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                EditText edit_name = findViewById(R.id.edit_name);
                EditText edit_age = findViewById(R.id.edit_age);
                Spinner edit_gender = findViewById(R.id.edit_gender);

                String name_str = edit_name.getText().toString();
                String age_str = edit_age.getText().toString();
                MapsActivity.last_maps.setup_name = name_str;
                MapsActivity.last_maps.setup_age = Integer.parseInt(age_str);
                MapsActivity.last_maps.setup_gender = edit_gender.getSelectedItemPosition() != 0;
                MapsActivity.last_maps.storeThis();
                MapsActivity.last_maps.startThread();
                finish();
            }
        });
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        if (requestCode == PICK_IMAGE) {
            try {
                InputStream input = getApplicationContext().getContentResolver().openInputStream(data.getData());
                Bitmap img = BitmapFactory.decodeStream(input);
                AppCompatImageView view = findViewById(R.id.profile_image_view);
                view.setImageBitmap(img);

                MapsActivity.last_maps.profile_image = img;
            }
            catch (FileNotFoundException e) {

            }
        }
    }

}
