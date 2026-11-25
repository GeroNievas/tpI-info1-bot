// https://github.com/GeroNievas/tpI-info1-bot
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <time.h>
#include <unistd.h>
#include <curl/curl.h>

struct memory {
char *response;
size_t size;
};

static size_t cb(char *data, size_t size, size_t nmemb, void *clientp)
{
size_t realsize = size * nmemb;
struct memory *mem = clientp;
char *ptr = realloc(mem->response, mem->size + realsize + 1);

if (!ptr)
return 0;

mem->response = ptr;
memcpy(&(mem->response[mem->size]), data, realsize);
mem->size += realsize;

mem->response[mem->size] = 0;
return realsize;
}

int consultar_url(CURL *curl, const char *url, struct memory *chunk)
{
CURLcode res;
chunk->response = NULL;
chunk->size = 0;

curl_easy_setopt(curl, CURLOPT_URL, url);
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);
res = curl_easy_perform(curl);

if (res != CURLE_OK)
{
printf("Error Codigo: %d\n", res);
return 0;
}

return 1;
}

void a_minusculas(const char *origen, char *destino)
{
int i = 0;
while (origen[i] != '\0')
{
destino[i] = (char)tolower((unsigned char)origen[i]);
i++;
}

destino[i] = '\0';
}

void codificar_url(const char *origen, char *destino)
{
int i = 0;
int j = 0;
while (origen[i] != '\0')
{
if (origen[i] == ' ')
{
strcpy(&destino[j], "%20");
j += 3;
}
else if (origen[i] == '&')
{
strcpy(&destino[j], "%26");
j += 3;
}
else if (origen[i] == '#')
{
strcpy(&destino[j], "%23");
j += 3;
}
else if (origen[i] == '?')
{
strcpy(&destino[j], "%3F");
j += 3;
}
else if (origen[i] == '+')
{
strcpy(&destino[j], "%2B");
j += 3;
}
else if (origen[i] == '"')
{
strcpy(&destino[j], "%22");
j += 3;
}
else
{
destino[j] = origen[i];
j++;
}
i++;
}

destino[j] = '\0';
}

void extraer_cadena_json(const char *inicio_clave, char *destino, int max_len)
{
const char *p = strchr(inicio_clave, ':');
int i = 0;
int j = 0;
destino[0] = '\0';

if (p == NULL)
return;

p = strchr(p, '"');
if (p == NULL)
return;

p++;
while (p[i] != '\0' && j < max_len - 1)
{
if (p[i] == '\\' && p[i + 1] == '"')
{
destino[j] = '"';
j++;
i += 2;
continue;
}
if (p[i] == '"')
break;

destino[j] = p[i];
j++;
i++;
}

destino[j] = '\0';
}

void registrar_log(long long hora_unix, const char *nombre, const char *mensaje)
{
FILE *f_log = fopen("log_bot.txt", "a");
if (f_log == NULL)
{
printf("Error al abrir log_bot.txt\n");
return;
}

fprintf(f_log, "Hora: %lld | Nombre: %s | Mensaje: %s\n", hora_unix, nombre, mensaje);
fclose(f_log);
}

void obtener_frase_zenquotes(CURL *curl, char *salida, int max_len)
{
struct memory zchunk = {0};
snprintf(salida, max_len, "No se pudo obtener la frase en este momento.");

if (consultar_url(curl, "https://zenquotes.io/api/random", &zchunk) && zchunk.response != NULL)
{
char *p_q = strstr(zchunk.response, "\"q\":");
char *p_a = strstr(zchunk.response, "\"a\":");
char frase[512] = "";
char autor[128] = "Anonimo";

if (p_q != NULL)
{
extraer_cadena_json(p_q, frase, sizeof(frase));
if (p_a != NULL)
extraer_cadena_json(p_a, autor, sizeof(autor));

snprintf(salida, max_len, "\"%s\" - %s", frase, autor);
}
}

free(zchunk.response);
}

int main(int argc, char *argv[])
{
if (argc < 2)
{
printf("Error: falta indicar el archivo con el token.\n");
printf("Uso: %s <archivo_token>\n", argv[0]);
return 1;
}

FILE *f_token = fopen(argv[1], "r");
if (f_token == NULL)
{
printf("Error: no se pudo abrir el archivo %s\n", argv[1]);
return 1;
}

char token[256];
if (fgets(token, sizeof(token), f_token) == NULL)
{
printf("Error: el archivo de token esta vacio.\n");
fclose(f_token);
return 1;
}

fclose(f_token);
size_t len = strlen(token);
while (len > 0 && (token[len - 1] == '\n' || token[len - 1] == '\r'))
{
token[len - 1] = '\0';
len--;
}

CURL *curl = curl_easy_init();
if (!curl)
{
printf("Error al inicializar curl.\n");
return 1;
}

long long ultimo_update_id = 0;
char url[2048];
struct memory chunk = {0};

printf("Bot activo. Esperando mensajes...\n");

while (1)
{
if (ultimo_update_id == 0)
snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates", token);
else
snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/getUpdates?offset=%lld", token, ultimo_update_id + 1);

if (consultar_url(curl, url, &chunk) && chunk.response != NULL)
{
char *cursor = strstr(chunk.response, "\"update_id\":");
while (cursor != NULL)
{
char *sig_update = strstr(cursor + 12, "\"update_id\":");
long long update_id = 0;
sscanf(cursor + 12, "%lld", &update_id);

if (update_id > ultimo_update_id)
ultimo_update_id = update_id;

char *p_chat = strstr(cursor, "\"chat\":");
char *p_date = strstr(cursor, "\"date\":");
char *p_text = strstr(cursor, "\"text\":");

if (p_chat != NULL && p_text != NULL && (sig_update == NULL || (p_chat < sig_update && p_text < sig_update)))
{
char *p_id = strstr(p_chat, "\"id\":");
char *p_name = strstr(p_chat, "\"first_name\":");
long long chat_id = 0;
long long hora_unix = (long long)time(NULL);

char nombre[128] = "Usuario";
char texto[512] = "";
char texto_min[512] = "";

if (p_id != NULL)
sscanf(p_id + 5, "%lld", &chat_id);

if (p_date != NULL && (sig_update == NULL || p_date < sig_update))
sscanf(p_date + 7, "%lld", &hora_unix);

if (p_name != NULL)
extraer_cadena_json(p_name, nombre, sizeof(nombre));

extraer_cadena_json(p_text, texto, sizeof(texto));
registrar_log(hora_unix, nombre, texto);
a_minusculas(texto, texto_min);

char respuesta[1024] = "";
int responder = 0;

if (strstr(texto_min, "hola") != NULL)
{
snprintf(respuesta, sizeof(respuesta), "Hola, %s", nombre);
responder = 1;
}
else if (strstr(texto_min, "chau") != NULL)
{
snprintf(respuesta, sizeof(respuesta), "Chau, %s", nombre);
responder = 1;
}
else if (strstr(texto_min, "frase") != NULL)
{
obtener_frase_zenquotes(curl, respuesta, sizeof(respuesta));
responder = 1;
}

if (responder && chat_id != 0)
{
char respuesta_cod[3072];
char url_envio[4096];
struct memory resp_chunk = {0};

codificar_url(respuesta, respuesta_cod);
snprintf(url_envio, sizeof(url_envio), "https://api.telegram.org/bot%s/sendMessage?chat_id=%lld&text=%s", token, chat_id, respuesta_cod);

if (consultar_url(curl, url_envio, &resp_chunk))
{
registrar_log((long long)time(NULL), "Bot", respuesta);
free(resp_chunk.response);
}
}
}

cursor = sig_update;
}
}

free(chunk.response);
chunk.response = NULL;
sleep(2);
}

curl_easy_cleanup(curl);
return 0;
}