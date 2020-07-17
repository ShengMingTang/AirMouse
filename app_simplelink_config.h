#ifndef APP_SIMPLELINK_CONFIG_H_
#define APP_SIMPLELINK_CONFIG_H_

void DisplayBanner(char * AppName);
void BoardInit(void);

void InitializeAppVariables();
long ConfigureSimpleLinkToDefaultState();
void ReadDeviceConfiguration();

signed long DisplayIP();

#endif /* APP_SIMPLELINK_CONFIG_H_ */
