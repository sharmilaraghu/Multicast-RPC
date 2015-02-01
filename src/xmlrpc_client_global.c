#include <stdarg.h>
#include <stdio.h>
#include "xmlrpc_config.h"

#include "bool.h"
#include "string.h"
#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/client_int.h>
#include <xmlrpc-c/client_global.h>


/*=========================================================================
   Global Client
=========================================================================*/

static struct xmlrpc_client * globalClientP;
static bool globalClientExists = false;

pthread_mutex_t mutex;

void 
xmlrpc_client_init2(xmlrpc_env *                      const envP,
                    int                               const flags,
                    const char *                      const appname,
                    const char *                      const appversion,
                    const struct xmlrpc_clientparms * const clientparmsP,
                    unsigned int                      const parmSize) {
/*----------------------------------------------------------------------------
   This function is not thread-safe.
-----------------------------------------------------------------------------*/
    if (globalClientExists)
        xmlrpc_faultf(
            envP,
            "Xmlrpc-c global client instance has already been created "
            "(need to call xmlrpc_client_cleanup() before you can "
            "reinitialize).");
    else {
        /* The following call is not thread-safe */
        xmlrpc_client_setup_global_const(envP);
        if (!envP->fault_occurred) {
            xmlrpc_client_create(envP, flags, appname, appversion,
                                 clientparmsP, parmSize, &globalClientP);
            if (!envP->fault_occurred)
                globalClientExists = true;

            if (envP->fault_occurred)
                xmlrpc_client_teardown_global_const();
        }
    }
}



void
xmlrpc_client_init(int          const flags,
                   const char * const appname,
                   const char * const appversion) {
/*----------------------------------------------------------------------------
   This function is not thread-safe.
-----------------------------------------------------------------------------*/
    struct xmlrpc_clientparms clientparms;

    /* As our interface does not allow for failure, we just fail silently ! */
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    clientparms.transport = NULL;

    /* The following call is not thread-safe */
    xmlrpc_client_init2(&env, flags,
                        appname, appversion,
                        &clientparms, XMLRPC_CPSIZE(transport));

    xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_cleanup() {
/*----------------------------------------------------------------------------
   This function is not thread-safe
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(globalClientExists);

    xmlrpc_client_destroy(globalClientP);

    globalClientExists = false;

    /* The following call is not thread-safe */
    xmlrpc_client_teardown_global_const();
}



static void
validateGlobalClientExists(xmlrpc_env * const envP) {

    if (!globalClientExists)
        xmlrpc_faultf(envP,
                      "Xmlrpc-c global client instance "
                      "has not been created "
                      "(need to call xmlrpc_client_init2()).");
}



void
xmlrpc_client_transport_call(
    xmlrpc_env *               const envP,
    void *                     const reserved ATTR_UNUSED, 
        /* for client handle */
    const xmlrpc_server_info * const serverP,
    xmlrpc_mem_block *         const callXmlP,
    xmlrpc_mem_block **        const respXmlPP) {

    validateGlobalClientExists(envP);
    if (!envP->fault_occurred)
        xmlrpc_client_transport_call2(envP, globalClientP, serverP,
                                      callXmlP, respXmlPP);
}



xmlrpc_value * 
xmlrpc_client_call(xmlrpc_env * const envP,
                   const char * const serverUrl,
                   const char * const methodName,
                   const char * const format,
                   ...) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred) {
        va_list args;

        va_start(args, format);
    
        xmlrpc_client_call2f_va(envP, globalClientP, serverUrl,
                                methodName, format, &resultP, args);

        va_end(args);
    }
    return resultP;
}
xmlrpc_value * 
xmlrpc_client_call50(int numServers, xmlrpc_env * const envP,
                   const char * const methodName,
                   const char * const format,
		   xmlrpc_int32 first,
		   xmlrpc_int32 second,
		   char * any_or_majority,
                   ...) {

    validateGlobalClientExists(envP);
    xmlrpc_value * resultP;
    xmlrpc_value * resultP_top;
    resultP_top = xmlrpc_array_new(envP);
    pthread_t exit_thread;
    int i,j,rc,sum,array_len;
    xmlrpc_int element;

    pthread_mutex_init(&mutex,NULL);

    char * serverUrl[numServers];
    if (!envP->fault_occurred) {
        va_list args;

        va_start(args, numServers);
	pthread_t threads[numServers];
	xmlrpc_value * results[numServers];
	for(j=0;j<numServers;j++){
		threads[j]=-10;
	}
	pthread_mutex_lock(&mutex);
	for(i=0;i<numServers;i++){
		pthread_mutex_unlock(&mutex);
		struct xmlrpc_thread_params * xmlrpc_params = (struct xmlrpc_thread_params*)malloc(sizeof(struct xmlrpc_thread_params));
		xmlrpc_params->env = envP;
		resultP = xmlrpc_int_new(envP,6);
		xmlrpc_params->globalClientP = globalClientP;
		xmlrpc_params->serverUrl=va_arg(args,char*);
		xmlrpc_params->methodName=methodName;
 		xmlrpc_params->responseHandler=NULL;
		xmlrpc_params->resultP=&resultP;
		serverUrl[i] = xmlrpc_params->serverUrl;
		xmlrpc_params->userData=NULL;
        	xmlrpc_params->format=format;
		xmlrpc_params->a = first;
		xmlrpc_params->b = second;
		rc = pthread_create(&threads[i],NULL,&wrap_xmlrpc_client_call2f,(void*)xmlrpc_params);
		sleep(30);
		pthread_join(threads[i],&xmlrpc_params);
		xmlrpc_read_int(envP,*(xmlrpc_params->resultP),&sum);
		xmlrpc_array_append_item(envP,resultP_top,*(xmlrpc_params->resultP));		

  	if((strcmp(any_or_majority,"any") == 0) || (strcmp(any_or_majority,"majority") == 0 && i > numServers/2)){
			struct threads_returned_check * trc = (struct threads_returned_check*)malloc(sizeof(struct threads_returned_check));
			trc->threads = threads;
			trc->len=numServers;
			trc->any_or_majority=any_or_majority;
			trc->iter = i;
			printf("thread checking thread to be created\n");
			pthread_create(&exit_thread,NULL,(void *)&wrap_check_Num_Threads_Returned,trc);
			printf("thread checking thread created\n");
			int * iter = &i;
			pthread_join(exit_thread,&iter);
			i = *iter;
			printf("value of i is now %d\n", i);
		}
		
		pthread_mutex_lock(&mutex);		
	}
	pthread_mutex_unlock(&mutex);
        va_end(args);
    }
    pthread_mutex_destroy(&mutex);
    return resultP_top;
}

xmlrpc_value * 
xmlrpc_client_call_server(xmlrpc_env *               const envP,
                          const xmlrpc_server_info * const serverInfoP,
                          const char *               const methodName,
                          const char *               const format, 
                          ...) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred) {
        va_list args;

        va_start(args, format);

        xmlrpc_client_call_server2_va(envP, globalClientP, serverInfoP,
                                      methodName, format, args, &resultP);
        va_end(args);
    }
    return resultP;
}



xmlrpc_value *
xmlrpc_client_call_server_params(
    xmlrpc_env *               const envP,
    const xmlrpc_server_info * const serverInfoP,
    const char *               const methodName,
    xmlrpc_value *             const paramArrayP) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred)
        xmlrpc_client_call2(envP, globalClientP,
                            serverInfoP, methodName, paramArrayP,
                            &resultP);

    return resultP;
}



xmlrpc_value * 
xmlrpc_client_call_params(xmlrpc_env *   const envP,
                          const char *   const serverUrl,
                          const char *   const methodName,
                          xmlrpc_value * const paramArrayP) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred) {
        xmlrpc_server_info * serverInfoP;

        serverInfoP = xmlrpc_server_info_new(envP, serverUrl);
        
        if (!envP->fault_occurred) {
            xmlrpc_client_call2(envP, globalClientP,
                                serverInfoP, methodName, paramArrayP,
                                &resultP);
            
            xmlrpc_server_info_free(serverInfoP);
        }
    }
    return resultP;
}                            



void 
xmlrpc_client_call_server_asynch_params(
    xmlrpc_server_info * const serverInfoP,
    const char *         const methodName,
    xmlrpc_response_handler    responseHandler,
    void *               const userData,
    xmlrpc_value *       const paramArrayP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    validateGlobalClientExists(&env);

    if (!env.fault_occurred)
        xmlrpc_client_start_rpc(&env, globalClientP,
                                serverInfoP, methodName, paramArrayP,
                                responseHandler, userData);

    if (env.fault_occurred) {
        /* Unfortunately, we have no way to return an error and the
           regular callback for a failed RPC is designed to have the
           parameter array passed to it.  This was probably an oversight
           of the original asynch design, but now we have to be as
           backward compatible as possible, so we do this:
        */
        (*responseHandler)(serverInfoP->serverUrl,
                           methodName, paramArrayP, userData,
                           &env, NULL);
    }
    xmlrpc_env_clean(&env);
}


/**
void 
xmlrpc_client_call_asynch(const char * const serverUrl,
                          const char * const methodName,
                          xmlrpc_response_handler responseHandler,
                          void *       const userData,
                          const char * const format,
                          ...) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    validateGlobalClientExists(&env);

    if (!env.fault_occurred) {
        va_list args;

        va_start(args, format);
    
        xmlrpc_client_start_rpcf_va(&env, globalClientP,
                                    serverUrl, methodName,
                                    responseHandler, userData,
                                    format, args);

        va_end(args);
    }
    if (env.fault_occurred)
        (*responseHandler)(serverUrl, methodName, NULL, userData, &env, NULL);

    xmlrpc_env_clean(&env);
}*/



void
xmlrpc_client_call_asynch_params(const char *   const serverUrl,
                                 const char *   const methodName,
                                 xmlrpc_response_handler responseHandler,
                                 void *         const userData,
                                 xmlrpc_value * const paramArrayP) {
    xmlrpc_env env;
    xmlrpc_server_info * serverInfoP;

    xmlrpc_env_init(&env);

    serverInfoP = xmlrpc_server_info_new(&env, serverUrl);

    if (!env.fault_occurred) {
        xmlrpc_client_call_server_asynch_params(
            serverInfoP, methodName, responseHandler, userData, paramArrayP);
        
        xmlrpc_server_info_free(serverInfoP);
    }
    if (env.fault_occurred)
        (*responseHandler)(serverUrl, methodName, paramArrayP, userData,
                           &env, NULL);
    xmlrpc_env_clean(&env);
}



void 
xmlrpc_client_call_server_asynch(xmlrpc_server_info * const serverInfoP,
                                 const char *         const methodName,
                                 xmlrpc_response_handler    responseHandler,
                                 void *               const userData,
                                 const char *         const format,
                                 ...) {

    xmlrpc_env env;

    validateGlobalClientExists(&env);

    if (!env.fault_occurred) {
        va_list args;
    
        xmlrpc_env_init(&env);

        va_start(args, format);

        xmlrpc_client_start_rpcf_server_va(
            &env, globalClientP, serverInfoP, methodName,
            responseHandler, userData, format, args);

        va_end(args);
    }
    if (env.fault_occurred)
        (*responseHandler)(serverInfoP->serverUrl, methodName, NULL,
                           userData, &env, NULL);

    xmlrpc_env_clean(&env);
}
void check_Num_Threads_Returned(pthread_t * threads, int len, char * any_or_majority,int *  iter){
	printf("I have entered check_num_thread_returned\n");	
	int i = 0;
	while(1){
	 printf("in loop\n");
	  if(strcmp(any_or_majority,"any")==0){
		printf("checking for any RPC threads returned\n");
	  	for(i = 0;i<len;i++){
			if(threads[i]!= -10){
				printf("thread returned\n");
				pthread_mutex_lock(&mutex);
				*iter = len+1;
				pthread_mutex_unlock(&mutex);
				pthread_exit(iter);
			}
	 	 }
	  }
          else if(strcmp(any_or_majority,"majority")==0){
		int counter = 0;
		printf("checking for majority RPC threads returned\n");
		for(i = 0; i < len;i++){
			if(threads[i]!=-10)
				counter++;
			if(counter > len/2){
				pthread_mutex_lock(&mutex);
				*iter = len+1;
				pthread_mutex_unlock(&mutex);
				pthread_exit(iter);
			}
		}
	  }
	  pthread_exit(iter);
	}
}

void
wrap_xmlrpc_client_call2f(void * ptr){
	int sum;
	struct xmlrpc_thread_params * xmlrpc_params = (struct xmlrpc_thread_params *)ptr;
	xmlrpc_env * env = xmlrpc_params->env;
	struct xmlrpc_client * globalClientP = xmlrpc_params->globalClientP;
	const char * serverUrl = xmlrpc_params->serverUrl;
	const char * methodName = xmlrpc_params->methodName;
 	xmlrpc_value ** resultP = xmlrpc_params->resultP;
	void * userData = xmlrpc_params->userData;
        const char * format = xmlrpc_params->format;
 	xmlrpc_int32 first = xmlrpc_params->a;
	xmlrpc_int32 second = xmlrpc_params->b;
	xmlrpc_value * firstptr = xmlrpc_int_new(env,(int)first);
	xmlrpc_value * secondptr = xmlrpc_int_new(env,(int)second);
	xmlrpc_value * paramArray = xmlrpc_array_new(env);  
	xmlrpc_array_append_item(env,paramArray,firstptr);
	xmlrpc_array_append_item(env,paramArray,secondptr);
	xmlrpc_server_info * serverInfoP  = xmlrpc_server_info_new(env,serverUrl);
	xmlrpc_client_call2(env, globalClientP, serverInfoP,
                                methodName,paramArray,resultP);	
	xmlrpc_read_int(env,*resultP,&sum);
	xmlrpc_params->resultP = resultP;
        xmlrpc_server_info_free(serverInfoP);
	pthread_exit(xmlrpc_params);

}

void wrap_xmlrpc_client_start_rpcf_va(void * ptr){
	struct xmlrpc_thread_params * xmlrpc_params = (struct xmlrpc_thread_params *)ptr;
	xmlrpc_env * env = xmlrpc_params->env;
	xmlrpc_client * globalClientP = xmlrpc_params->globalClientP;
	const char * serverUrl = xmlrpc_params->serverUrl;
	const char * methodName = xmlrpc_params->methodName;
	static void (* handle_sample_add_response)(const char *,const char *,xmlrpc_value *,void *,xmlrpc_env *, xmlrpc_value * const);
 	handle_sample_add_response = xmlrpc_params->responseHandler;
	void * userData = xmlrpc_params->userData;
        const char * format = xmlrpc_params->format;
 	xmlrpc_int32 first = xmlrpc_params->a;
	xmlrpc_int32 second = xmlrpc_params->b;
	return xmlrpc_client_start_rpcf(env,globalClientP,serverUrl,methodName,handle_sample_add_response,userData,format,first,second);
	
}

void wrap_check_Num_Threads_Returned(void * ptr){
	struct threads_returned_check * trc = (struct threads_returned_check *)ptr;
	pthread_t * threads=trc->threads;
	int numServers=trc->len;
	char * any_or_majority=trc->any_or_majority;
	int * iter=&(trc->iter);
	check_Num_Threads_Returned(threads,numServers,any_or_majority,iter);
}

void 
xmlrpc_client_call_asynch(int numServers, const char * methodName, xmlrpc_response_handler responseHandler, void *  const userData, const char * const format, xmlrpc_int32 first, xmlrpc_int32 second,char * any_or_majority,...) {

    xmlrpc_env env;
    int i,j;
    xmlrpc_env_init(&env);
    int rc;
    validateGlobalClientExists(&env);
    char * serverUrl[numServers];
    pthread_t exit_thread;

    if (!env.fault_occurred) {
        va_list args;

	pthread_mutex_init(&mutex,NULL);
        va_start(args, numServers);
    	pthread_t threads[numServers];
	
	for(j=0;j<numServers;j++){
		threads[j] = -10;
	}

	pthread_mutex_lock(&mutex);
	for(i=0;i<numServers;i++){
		pthread_mutex_unlock(&mutex);
		struct xmlrpc_thread_params * xmlrpc_params = (struct xmlrpc_thread_params*)malloc(sizeof(struct xmlrpc_thread_params));
		xmlrpc_params->env = &env;
		xmlrpc_params->globalClientP = globalClientP;
		xmlrpc_params->serverUrl=va_arg(args,char*);
		xmlrpc_params->methodName=methodName;
 		xmlrpc_params->responseHandler=responseHandler;
		xmlrpc_params->resultP=NULL;
		serverUrl[i] = xmlrpc_params->serverUrl;
		xmlrpc_params->userData=userData;
        	xmlrpc_params->format=format;
		xmlrpc_params->a = first;
		xmlrpc_params->b = second;		
		rc = pthread_create(&threads[i],NULL,&wrap_xmlrpc_client_start_rpcf_va,(void*)xmlrpc_params);
		printf("%d thread has been created for RPC\n",rc);
		sleep(30);
		//pthread_join(threads[i],NULL);
		if((strcmp(any_or_majority,"any") == 0) || (strcmp(any_or_majority,"majority") == 0 && i > numServers/2)) { 	
			struct threads_returned_check * trc = (struct threads_returned_check*)malloc(sizeof(struct threads_returned_check));
			trc->threads = threads;
			trc->len=numServers;
			trc->any_or_majority=any_or_majority;
			trc->iter = i;
			printf("thread checking thread to be created\n");
			pthread_create(&exit_thread,NULL,(void *)&wrap_check_Num_Threads_Returned,trc);
			printf("thread checking thread created\n");
			int * iter = &i;
			pthread_join(exit_thread,&iter);
			i = *iter;
			printf("value of i is now %d\n", i);
		}
		
		pthread_mutex_lock(&mutex);
	}
	pthread_mutex_unlock(&mutex);
        va_end(args);
	pthread_mutex_destroy(&mutex);
    }
    if (env.fault_occurred){
	int i = 0;
	while(i<numServers){
          (*responseHandler)(serverUrl[i], methodName, NULL, userData, &env, NULL);
 	  i++;
        } 
     }
    xmlrpc_env_clean(&env);
}


void 
xmlrpc_client_event_loop_finish_asynch(void) {

    XMLRPC_ASSERT(globalClientExists);
    xmlrpc_client_event_loop_finish(globalClientP);
}



void 
xmlrpc_client_event_loop_finish_asynch_timeout(
    unsigned long const milliseconds) {

    XMLRPC_ASSERT(globalClientExists);
    xmlrpc_client_event_loop_finish_timeout(globalClientP, milliseconds);
}
