#ifndef IMVW_APP_INIT_H
#define IMVW_APP_INIT_H

// Called once at startup.  Handles DPI awareness, precision timer, window
// creation, SSAA init, font/shader loading, flut registration, icon setup,
// initial image load, camera home, and Python startup scripts.
void imvw_init(int argc, char **argv);

// Called once at shutdown.  Releases SSAA resources and closes the window.
void imvw_cleanup(void);

#endif //IMVW_APP_INIT_H
