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
import android.widget.TextView;

import java.io.FileNotFoundException;
import java.io.InputStream;

public class OnboardingActivity extends Activity {

    public static final int PICK_IMAGE = 1;

    public OnboardingActivity() {
        AppService.profile_image = AppService.randomUserImage();

    }

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.onboarding1);

        Button next1 = (Button)findViewById(R.id.next1);
        next1.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                SetPage1b();
            }
        });


    }

    void SetPage1b() {
        setContentView(R.layout.onboarding1b);

        Button next1b = (Button)findViewById(R.id.next1b);
        next1b.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                SetPage2();
            }
        });

        TextView privacypolicy = findViewById(R.id.privacypolicy);
        String text = "";
        text = text + "F2F Privacy Policy\n";
        text = text + "Last updated: 11.10.2018\n";
        text = text + "\n";
        text = text + "Team F2F (\"us\", \"we\", or \"our\") operates the android application F2F (the \"Application\"). This page informs you of our policies regarding the collection, use and disclosure of usage data we receive from users of the F2F android application.\n";
        text = text + "\n";
        text = text + "We use your Application usage data only for providing and improving the Application. By using the Application, you agree to the collection and use of information in accordance with this policy.\n";
        text = text + "\n";
        text = text + "Information Collection And Use\n";
        text = text + "While using our Application, we don't ask you to provide us any personally identifiable information. We collect your nickname, approximated age, gender and location. The information is used for server usage analysis, which includes generating heatmaps that shows where people have been.\n";
        text = text + "\n";
        text = text + "Log Data\n";
        text = text + "Like many Android application operators, we collect information that your smart device sends whenever you use our Application (\"Log Data\").\n";
        text = text + "\n";
        text = text + "This Log Data may include information such as your smart device's Internet Protocol (\"IP\") address, the time and date of your visit, the time spent using the Application and other statistics.\n";
        text = text + "\n";
        text = text + "Security\n";
        text = text + "As we don't collect any personally identifiable information, we do not need to encrypt our traffic according to Play store rules. We do not encrypt the traffic of the Application yet, but it will be a feature of the future version of the Application.\n";
        text = text + "\n";
        text = text + "Changes To This Privacy Policy\n";
        text = text + "This Privacy Policy is effective as of 11.10.2018 and will remain in effect except with respect to any changes in its provisions in the future, which will be in effect immediately after being posted on this page.\n";
        text = text + "\n";
        text = text + "We reserve the right to update or change our Privacy Policy at any time and you should check this Privacy Policy periodically. Your continued use of the Application after we post any modifications to the Privacy Policy on this page will constitute your acknowledgment of the modifications and your consent to abide and be bound by the modified Privacy Policy.\n";
        text = text + "\n";
        text = text + "If we make any material changes to this Privacy Policy, we will notify you either through the email address you have provided us, or by placing a prominent notice on our website.\n";
        text = text + "\n";
        text = text + "Contact Us\n";
        text = text + "If you have any questions about this Privacy Policy, please contact us. (email seppo.pakonen@gmail.com)";
        privacypolicy.setText(text);
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
        view.setImageBitmap(AppService.profile_image);

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
                MapsActivity.last.setupFinish(name_str, Integer.parseInt(age_str), edit_gender.getSelectedItemPosition() != 0);
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

                MapsActivity.last.startSetProfileImage(img);
            }
            catch (FileNotFoundException e) {

            }
        }
    }

}
