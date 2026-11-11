#include <X11/X.h>
#include <X11/Xutil.h>

#include "langext.h"
#include "library.h"
#include "string.h"
#include "syscall.h"
#include "libc.h"
#include "math.h"

static void printNumber(int number){
	int length = 0;
	int length_copy;
	char buffer[0x10];
    bool negative = false;
	int number_copy;

    if(number < 0){
        length++;
        negative = true;
        number = -number;
    }
	number_copy = number;

    if(!number)
        length = 1;
    else{
        while(number_copy){
            number_copy /= 10;
            length++;
        }
    }
	length_copy = length;

    while(length--){
        buffer[length] = number % 10 + '0';
        number /= 10;
    }
    if(negative)
        buffer[0] = '-';
    buffer[length_copy] = 0;

    systemWrite(1,buffer,length_copy);
}

structure(curlList){
    char* data;
    curlList* next;
};

#define CURL_GLOBAL_DEFAULT 3

#define CURLOPT_POST 47
#define CURLOPT_URL 10002
#define CURLOPT_HTTPHEADER 10023
#define CURLOPT_POSTFIELDS 10015
#define CURLOPT_WRITEFUNCTION 20011

static int (*curlGlobalInit)(long flags);
static void* (*curlEasyInit)(void);
static int (*curlEasySetopt)(void* handle,int option,...);
static int (*curlEasyPerform)(void* handle);
static void (*curlEasyCleanup)(void* handle);

#define MAP_SIZE 0x20

int map[MAP_SIZE * MAP_SIZE];

static int current_index;

static String json;

#define WINDOW_SIZE_X 0x400
#define WINDOW_SIZE_Y 0x400

static int buffer[WINDOW_SIZE_X * WINDOW_SIZE_Y];

static size_t llmCallback(void *contents,size_t size,size_t nmemb,void *userp){
    String llm_data = {.data = contents,.size = nmemb};
    String query = STRING_LITERAL("content");
    String filter = stringInString(llm_data,query);
    filter = stringForwardSlice(filter,query.size + 3);
    if(filter.size < 6 * 4 * 4){
        stringPrint((String)STRING_LITERAL("llm meshed up"));
        stringPrint(llm_data);
        stringPrint((String)STRING_LITERAL("\n"));
        return size * nmemb;
    }
    int index = 0;
    for(int j = 0;j < 4*4;j++){
        int color = 0;
        for(int k = 6;k--;){
            color *= 0x10;
            if(filter.size < index){
                stringPrint((String)STRING_LITERAL("llm meshed up"));
                stringPrint(llm_data);
                stringPrint((String)STRING_LITERAL("\n"));
                return size * nmemb;
            }
            char v = filter.data[index++];
            if(v >= '0' && v <= '9')
                color += v - '0';
            else if(v >= 'A' && v <= 'F')
                color += v - 'A' + 0x0A;
            else
                k++;
        }
        int x = (current_index / MAP_SIZE + j / 4 - 1 + MAP_SIZE) % MAP_SIZE;
        int y = (current_index % MAP_SIZE + j % 4 - 1 + MAP_SIZE) % MAP_SIZE;
        map[x * MAP_SIZE + y] = color;
    }
    for(int j = countof(map);j--;){
        for(int i = 8 * 8;i--;){
            int mx = (j / MAP_SIZE) * 8 + i / 8;
            int my = (j % MAP_SIZE) * 8 + i % 8;
            buffer[mx * WINDOW_SIZE_Y + my] = map[j];//(color >> 16 & 0x0000FF)| (color & 0x00FF00) | (color << 16 & 0xFF0000);
        }
    }
    stringPrint(llm_data);
    stringPrint((String)STRING_LITERAL("\n"));
    return size * nmemb;
}

static void shuffle(int* arr,int size){
    for(int i = size - 1;i > 0;i--){
        int j = mathRnd() % (i + 1);

        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

static Visual* visual;
static GC context;
static XEvent event;
static XImage* image;
static size_t window;
static int screen;
static void* display;

static void frameRender(void){
    XImage* image = XCreateImage(display,visual,DefaultDepth(display,screen),ZPixmap,0,(void*)buffer,WINDOW_SIZE_X,WINDOW_SIZE_Y,32,0);
    XPutImage(display,window,context,image,0,0,0,0,WINDOW_SIZE_X,WINDOW_SIZE_Y);
    while(XPending(display)){
        XNextEvent(display,&event);
        switch(event.type){
                
        }
    }
    XFlush(display);
    image->data = 0;
    XDestroyImage(image);
}

int main(void){
    #if 1
    for(int i = countof(map);i--;)
        map[i] = 0x10000000;
    #endif
    void* curl_lib = libraryLoad("libcurl.so");
    struct{
        char* name;
        funcptr_t* fn_ptr;
    } functions[] =  {
        {.name = "curl_global_init",.fn_ptr = (funcptr_t*)&curlGlobalInit},
        {.name = "curl_easy_init",.fn_ptr = (funcptr_t*)&curlEasyInit},
        {.name = "curl_easy_setopt",.fn_ptr = (funcptr_t*)&curlEasySetopt},
        {.name = "curl_easy_perform",.fn_ptr = (funcptr_t*)&curlEasyPerform},
        {.name = "curl_easy_cleanup",.fn_ptr = (funcptr_t*)&curlEasyCleanup},
    };
    for(int i = countof(functions);i--;){
        *functions[i].fn_ptr = libraryFunctionLoad(curl_lib,functions[i].name);
    }
    curlGlobalInit(CURL_GLOBAL_DEFAULT);
    display = XOpenDisplay(NULL);

    if(!display)
        return 0;

    screen = DefaultScreen(display);
    
    window = XCreateSimpleWindow(
        display,
        XDefaultRootWindow(display),
        10,10,
        WINDOW_SIZE_X,WINDOW_SIZE_Y,
        0,
        0,
        0
    );

    XMapWindow(display,window);
    XFlush(display);

    visual = DefaultVisual(display,screen);
    context = XDefaultGC(display,DefaultScreen(display));

    static int indices[countof(map)];
    for(int i = countof(map);i--;)
        indices[i] = i;
    shuffle(indices,countof(map));
    for(;;){
        for(int i = countof(map);i--;){
            int index = indices[i];
            current_index = index;
            void* request = curlEasyInit();
            curlEasySetopt(request,CURLOPT_URL,"http://localhost:11434/api/chat");
            curlEasySetopt(request,CURLOPT_WRITEFUNCTION,llmCallback);
            curlList header_list = {.data = "Content-Type: application/json"};
            curlEasySetopt(request,CURLOPT_HTTPHEADER,&header_list);
            curlEasySetopt(request,CURLOPT_POST,(long)true);
            json = (String)STRING_LITERAL("{\"model\": \"gemma4:26b\",\"messages\": [{\"role\": \"user\", \"content\": \"you are making a chessboard, your task is to generate the hidden pixels at the ? symbols, ");
            json = stringConcat(0,json,(String)STRING_LITERAL("2D local representation, you fill the middle: "));
#if 1
            int width = 16;
            json = stringConcatChar(0,json,'{');
            for(int j = 0;j < width * width;j++){
                int x = ((index / MAP_SIZE + j / width) + MAP_SIZE - width / 2 - 1) % MAP_SIZE;
                int y = ((index % MAP_SIZE + j % width) + MAP_SIZE - width / 2 - 1) % MAP_SIZE;
                if((j / width >= 6 && j / width <= 9) && (j % width >= 6 && j % width <= 9)){
                    json = stringConcat(0,json,(String)STRING_LITERAL("?"));
                    json = stringConcatChar(0,json,',');
                    continue;
                }
                if(map[x * MAP_SIZE + y] & 0x10000000){
                    json = stringConcatChar(0,json,'!');
                    if(j % width == width - 1)
                        json = stringConcatChar(0,json,'}');
                    json = stringConcatChar(0,json,',');
                    if(j % width == width - 1)
                        json = stringConcatChar(0,json,'{');
                    continue;
                }
                for(int k = 6;k--;){
                    int v = map[x * MAP_SIZE + y] >> k * 4 & 0x0F;
                    char c;
                    if(v > 0x09)
                        c = v + 'A' - 0x0A;
                    else
                        c = v + '0';
                    json = stringConcatChar(0,json,c);
                }
                if(j % width == width - 1)
                    json = stringConcatChar(0,json,'}');
                json = stringConcatChar(0,json,',');
                if(j / width != width - 1){
                    if(j % width == width - 1)
                        json = stringConcatChar(0,json,'{');
                }
                
            }
#endif
            json = stringConcat(0,json,(String)STRING_LITERAL("color is represented as hexadecimal,only answer with 4X4 six char, so 16 times, hexadecimal colors yourself in exactly this format: color,color,etc; use only uppercase, Output exactly 16 six-character hexadecimal RGB colors, in row-major order, separated by commas, ! means color is not yet placed\"}],\"stream\": false,\"think\": false}"));
            json = stringConcatChar(0,json,'\0');
            curlEasySetopt(request,CURLOPT_POSTFIELDS,json.data);
            curlEasyPerform(request);
            curlEasyCleanup(request);
#if 0
            stringPrint(json);
            stringPrint((String)STRING_LITERAL("\n"));
#endif
            frameRender();
        }
    }
    for(;;){
        frameRender();
        systemNanoSleep(&(TimeSpec){.nano_seconds = 10000000,.seconds = 0},0);
    }
    return 0;
}
