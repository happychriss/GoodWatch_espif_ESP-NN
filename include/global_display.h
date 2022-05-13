//
// Created by development on 18.10.21.
//

#ifndef GOODWATCH_GLOBAL_DISPLAY_H
#define GOODWATCH_GLOBAL_DISPLAY_H


#define PT18_HEIGHT 39
#define PT18_WIDTH 32

#define PT12_HEIGHT 28
#define PT12_BREAK (PT12_HEIGHT/2)
#define PT12_WIDTH 20

#define ALARM_NUMBERS_DISPLAY 5
#define TIME_COLUMN 7 // which column the time to display
#define WAKE_HOURS_COLUMN (TIME_COLUMN + 5)
#define CONFIRM_LINE (10*PT12_HEIGHT) // which line to show confirmation message
#define MESSAGE_LINE (1*PT12_HEIGHT) // which line to show welcome message, instructions


#define MESSAGE_ANSWER_COLUMN  15
#define ERROR_CHAR 'x'
#define INPUT_PROMPT '?'
#define EMPTY_PROMPT '-'
#define OFF_STRING "(aus)"

#define TIME_START "?-:--"
#define TIME_EMPTY "--:--"
#include "WString.h"


// see model_metadata.h - indes of labels from { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "j", "n" };
#define ANSWER_YES 10
#define ANSWER_NO 11
#define ANSWER_SET 9


enum enum_alarm_type {
    single, repeating, bike
};

struct str_watch_config {
    int days[7];
    enum_alarm_type type;
    int dl; //line on the display
    String alarm_name;

};

// days start from Sunday: Sun,M,T,T,F,Sat



#endif //GOODWATCH_GLOBAL_DISPLAY_H
