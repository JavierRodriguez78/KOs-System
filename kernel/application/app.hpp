// Prototypes for application command entrypoints (shared between kernel and standalone builds)
#ifndef KOS_APPLICATION_APP_HPP
#define KOS_APPLICATION_APP_HPP

extern "C" void app_hello();
extern "C" void app_echo();
extern "C" void app_ls();
extern "C" void app_pwd();

#endif // KOS_APPLICATION_APP_HPP