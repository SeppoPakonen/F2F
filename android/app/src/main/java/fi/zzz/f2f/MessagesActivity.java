package fi.zzz.f2f;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.widget.ListView;

public class MessagesActivity extends Activity {

    private EditText editText;
    private MessageAdapter messageAdapter;
    private ListView messagesView;

    public static MessagesActivity last;

    public MessagesActivity() {
        last = this;

    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        last = this;

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_messages);

        editText = (EditText) findViewById(R.id.editText);

        messageAdapter = new MessageAdapter(this);
        messagesView = (ListView) findViewById(R.id.messages_view);
        messagesView.setAdapter(messageAdapter);

        postAddMessages();
    }

    void postAddMessages() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                MapsActivity.last.startAddMessages();
            }
        });
    }

    void addMessages(Channel ch) {
        if (messagesView.getCount() != 0)
            return;
        //Channel ch = AppService.last.getActiveChannel();
        for (ChannelMessage msg : ch.messages) {
            final GuiMessage m = new GuiMessage(msg.message, msg.sender_name, "#000000", msg.belongs_to_user, msg.icon);
            messageAdapter.add(m);
        }

        // Scroll to end
        messagesView.setSelection(messagesView.getCount() - 1);
    }

    public void sendMessage(View view) {
        String message = editText.getText().toString();
        if (message.length() > 0) {
            //scaledrone.publish("observable-room", message);

            editText.getText().clear();

            // since the message body is a simple string in our case we can use json.asText() to parse it as such
            // if it was instead an object we could use a similar pattern to data parsing
            final GuiMessage msg = new GuiMessage(message, "Me", "#FF0000", true, null);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    messageAdapter.add(msg);
                    // scroll the ListView to the last added element
                    messagesView.setSelection(messagesView.getCount() - 1);
                }
            });

            MapsActivity.last.startSendMessage(message);
        }
    }

    public void gotMessage(String sender, String message, Bitmap icon) {
        final GuiMessage msg = new GuiMessage(message, sender, "#0000FF", false, icon);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                messageAdapter.add(msg);
                // scroll the ListView to the last added element
                messagesView.setSelection(messagesView.getCount() - 1);
            }
        });
    }
}

