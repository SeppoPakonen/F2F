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

public class SettingsActivity extends Activity {

    private Bitmap profile_image;
    public static final int PICK_IMAGE = 1;

    public SettingsActivity() {


    }

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.settings);

        profile_image = AppService.profile_image;
        AppCompatImageView view = findViewById(R.id.profile_image_view2);
        view.setImageBitmap(profile_image);
        view.setOnClickListener(new View.OnClickListener() {
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

        EditText edit_name = findViewById(R.id.edit_name2);
        EditText edit_age = findViewById(R.id.edit_age2);
        Spinner edit_gender = findViewById(R.id.edit_gender2);
        edit_name.setText(AppService.setup_name);
        edit_age.setText(Integer.toString(AppService.setup_age));

        String[] items = new String[]{"Female", "Male"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, items);
        edit_gender.setAdapter(adapter);
        edit_gender.setSelection(AppService.setup_gender ? 1 : 0);

        Button save = (Button)findViewById(R.id.save);
        save.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                save();
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
                AppCompatImageView view = findViewById(R.id.profile_image_view2);
                view.setImageBitmap(img);

                profile_image = img;
            }
            catch (FileNotFoundException e) {

            }
            catch (NullPointerException e) {

            }
        }
    }
    private void save() {

        EditText edit_name = findViewById(R.id.edit_name2);
        EditText edit_age = findViewById(R.id.edit_age2);
        Spinner edit_gender = findViewById(R.id.edit_gender2);

        String name_str = edit_name.getText().toString();
        String age_str = edit_age.getText().toString();
        MapsActivity.last.settingsFinish(
                name_str,
                Integer.parseInt(age_str),
                edit_gender.getSelectedItemPosition() != 0,
                profile_image);
        finish();
    }


}
