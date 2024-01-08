#define MSTAR            1
#define ALLWINNER        2
#define ALLWINNER_V2     3
#define ALLWINNER_V2_ALT 4

int is_ptz(char *model);
int get_model_suffix(char *model, int size);
int read_ptz(int *px, int *py, int *pi);
