package fi.zzz.f2f;

import android.graphics.Bitmap;
import android.graphics.Color;

public class GuiMessage {
    private String text; // message body
    private MemberData data; // data of the user that sent this message
    private boolean belongsToCurrentUser; // is this message sent by us?
    private String name;
    private String color;
    private Bitmap icon;

    public class MemberData {
        String name;
        String clr;

    }

    public GuiMessage(String text, String name, String color, boolean belongsToCurrentUser, Bitmap icon) {
        this.text = text;
        this.name = name;
        this.color = color;
        this.belongsToCurrentUser = belongsToCurrentUser;
        this.icon = icon;
    }


    String getColor() {return color;}
    String getName() {return name;}

    Bitmap getIcon() {return icon;}

    public String getText() {
        return text;
    }

    public MemberData getData() {
        return data;
    }

    public boolean isBelongsToCurrentUser() {
        return belongsToCurrentUser;
    }
}
