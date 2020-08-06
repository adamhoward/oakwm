#include "toolbar.hh"

#define LOG_NAME "toolbar"

#include "git-rev.hh"
#include "icon.cc"
#include "log.hh"
#include "util.hh"

/**
 * main()
 *
 * The main entry point into the program.
 *
 * @param arc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @return The return code of the program.
 **/
int main(int argc, char **argv){
  /* Dump version information */
  LOGM("Build Date: ",   STR(BUILD_DATE));
  LOGM("Build Branch: ", STR(GIT_BRANCH_RAW));
  LOGM("Build Hash: ",   STR(GIT_HASH_RAW));
  /* Check command line parameters */
  char* config = NULL;
  if(argc == 2){
    config = argv[1];
  }
  /* Start toolbar */
  LOG("Starting okawm toolbar...");
  Toolbar* toolbar = new Toolbar(config);
  /* Exit success */
  return 0;
}

Toolbar::Toolbar(char* filename){
  /* Get settings if provided */
  if(filename != NULL){
    LOG("Loading configuration...");
    json = JSON::build(filename);
  }else{
    LOG("No configuration provided");
    json = new JSON("{}");
  }
  /* Create the X connection */
  dis = XOpenDisplay(NULL);
  if(!dis){
    WARN("Unable to open display, program may crash");
  }
  screen = DefaultScreen(dis);
  int width = XWidthOfScreen(ScreenOfDisplay(dis, screen));
  /* Get default colours from X11 */
  unsigned long background = Util::strToLong(
    json->get("colours")->get("background")->value("FFFFFF").c_str(),
    16
  );
  unsigned long foreground = Util::strToLong(
    json->get("colours")->get("foreground")->value("000000").c_str(),
    16
  );
  /* Setup window attributes (remove border) */
  XSetWindowAttributes attr;
  attr.override_redirect = 1;
  /* Create the window for the given display */
  win = XCreateWindow(
    dis,
    RootWindow(dis, screen),
    0,
    0,
    width,
    std::atoi(json->get("toolbar")->get("height")->value("32").c_str()),
    0,
    CopyFromParent,
    CopyFromParent,
    CopyFromParent,
    CWOverrideRedirect,
    &attr
  );
  /* Set window properties */
  XSetWindowBackground(dis, win, background);
  /* Select allowed input methods */
  XSelectInput(
    dis,
    win,
    ButtonPressMask   |
    ButtonReleaseMask |
    ExposureMask      |
    PointerMotionMask |
    VisibilityChangeMask
  );
  /* Create graphics context */
  gc = XCreateGC(dis, win, 0, 0);
  /* Set window colours */
  XSetBackground(dis, gc, background);
  XSetForeground(dis, gc, foreground);
  /* Clear the window and bring it on top of the other windows */
  XClearWindow(dis, win);
  XMapRaised(dis, win);
  /* Generate some icons */
  int leftOff = 0;
  int rightOff = width;
  for(int x = 0; x < json->get("toolbar")->get("icons")->length(); x++){
    JSON* iCfg = json->get("toolbar")->get("icons")->get(x);
    bool alignLeft = iCfg->get("align")->value("left").compare("left") == 0;
    Icon* icon = new Icon(
      iCfg->get("image")->value(""),
      0,
      0,
      iCfg->get("interactive")->value("false").compare("true") == 0,
      dis,
      win,
      gc
    );
    if(alignLeft){
      icon->setXY(leftOff, 0);
      leftOff += icon->getWidth();
    }else{
      rightOff -= icon->getWidth();
      icon->setXY(rightOff, 0);
    }
    icons.emplace_back(icon);
  }
  /* Apply icon modifiers */
  for(int x = 0; x < json->get("toolbar")->get("modifiers")->length(); x++){
    JSON* mCfg = json->get("toolbar")->get("modifiers")->get(x);
    std::string name = mCfg->get("name")->value("");
    unsigned long mask = Util::strToLong(mCfg->get("mask")->value("").c_str(), 16);
    /* Update our icons with modifiers */
    for(int z = 0; z < icons.size(); z++){
      icons[z]->addModifier(name, mask, dis);
    }
  }
  /* Enter main loop */
  loop();
}

Toolbar::~Toolbar(){
  XFreeGC(dis, gc);
  XDestroyWindow(dis, win);
  XCloseDisplay(dis);
  exit(1);
}

void Toolbar::loop(){
  XEvent event;
  while(true){
    /* Get window events */
    XNextEvent(dis, &event);
    /* Check and handle event type */
    int mouseX = -1;
    int mouseY = -1;
    int press = false;
    int type = event.type;
    switch(type){
      case Expose :
        if(event.xexpose.count == 0){
          redraw();
        }
        break;
      case VisibilityNotify :
        /* TODO: This is a hack, two or more windows doing this would fight. */
        XRaiseWindow(dis, win);
        XFlush(dis);
        break;
      case ButtonPress :
      case ButtonRelease :
        mouseX = event.xbutton.x;
        mouseY = event.xbutton.y;
        press = type == ButtonPress;
        break;
      case MotionNotify :
        mouseX = event.xmotion.x;
        mouseY = event.xmotion.y;
        press = event.xmotion.state != 0;
        break;
      default :
        WARN("Unhandled event triggered");
        break;
    }
    /* Update icon states if mouse event occurred */
    if(mouseX >= 0 && mouseY >= 0){
      for(int i = 0; i < icons.size(); i++){
        if(icons[i]->interactive() && icons[i]->insideBounds(mouseX, mouseY)){
          /* Is it hover of press related? */
          if(type == MotionNotify && !press){
            icons[i]->setFocused(true);
          }else{
            icons[i]->setActive(press);
          }
        }else{
          icons[i]->setFocused(false);
        }
      }
      /* As we moused over something, redraw to be safe */
      redraw();
    }
    /* TODO: Handle main logic. */
  }
}

void Toolbar::redraw(){
  /* Loop icons */
  for(int x = 0; x < icons.size(); x++){
    icons[x]->draw(dis, win, gc);
  }
}
