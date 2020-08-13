#include <assert.h>
#include <curl/curl.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define eprintf(...) fprintf(stderr,__VA_ARGS__)

char *buf=NULL;
size_t sz=0;

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata){

  assert(size==1);
  assert(nmemb>=1);

  // eprintf("new segment\n");

  assert(NULL!=(buf=realloc(buf,sz+nmemb)));
  memcpy(buf+sz,ptr,nmemb);
  sz+=nmemb;

  return nmemb;

}

void assert_field(const json_object *const j,const char *const k,const char *const v){
  json_object *p=NULL;
  assert(json_object_object_get_ex(j,k,&p));
  assert(p);
  assert(json_type_string==json_object_get_type(p));
  assert(0==strcmp(v,json_object_get_string(p)));
  // assert(1==json_object_put(p));
  // p=NULL;
}

const char *get_field(const json_object *const j,const char *const k){
  json_object *p=NULL;
  assert(json_object_object_get_ex(j,k,&p));
  assert(p);
  assert(json_type_string==json_object_get_type(p));
  return json_object_get_string(p);
}

int main(){

  // printf("%s\n",curl_version());
  // assert(0==curl_global_init(CURL_GLOBAL_NOTHING));

  CURL *curl=curl_easy_init();
  assert(curl);

  assert(CURLE_OK==curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,write_callback));
  assert(CURLE_OK==curl_easy_setopt(curl,CURLOPT_HEADER,0L));
  // assert(CURLE_OK==curl_easy_setopt(curl,CURLOPT_URL,"http://127.0.0.1:6170/proxies"));
  assert(CURLE_OK==curl_easy_setopt(curl,CURLOPT_URL,"http://127.0.0.1:6170/proxies/GLOBAL"));
  // assert(CURLE_OK==curl_easy_setopt(curl,CURLOPT_URL,"http://127.0.0.1:6170/proxies/JP"));
  // assert(CURLE_OK==curl_easy_setopt(curl,CURLOPT_URL,"http://127.0.0.1:6170/proxies/香港标准 IEPL 中继 19"));
  assert(CURLE_OK==curl_easy_perform(curl));
  // printf("%.*s\n",(int)sz,buf);
  assert(NULL!=(buf=realloc(buf,++sz)));
  buf[sz-1]='\0';
  // printf("%s\n",buf);

  json_tokener *tok=json_tokener_new();
  assert(tok);
  json_object *jobj=json_tokener_parse_ex(tok,buf,-1);
  enum json_tokener_error jerr=json_tokener_get_error(tok);
  // printf("%s\n",json_tokener_error_desc(jerr));
  assert(jerr==json_tokener_success);
  json_tokener_free(tok);
  tok=NULL;

  assert_field(jobj,"name","GLOBAL");
  assert_field(jobj,"type","Selector");
  printf("%s\n",get_field(jobj,"now"));

  // printf("%s\n",json_object_to_json_string_ext(jobj,JSON_C_TO_STRING_PLAIN|JSON_C_TO_STRING_SPACED));
  // printf("%s\n",json_object_to_json_string_ext(jobj,JSON_C_TO_STRING_PRETTY));

  assert(1==json_object_put(jobj));
  jobj=NULL;
  free(buf);
  buf=NULL;
  sz=0;
  curl_easy_cleanup(curl);
  curl=NULL;
  curl_global_cleanup();

  return 0;

}
