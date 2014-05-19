/* 
 ** Java-Bridge NetXMS subagent
 ** Copyright (C) 2013 TEMPEST a.s.
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 **
 ** File: ConfigHelper.cpp
 **
 **/

#include "java_subagent.h"
#include "ConfigHelper.h"
#include "JNIException.h"

/* Provides implementations of JNI methods for Config and ConfigEntry
 ** Implementation of object wrapper and Java native methods
 **/

namespace org_netxms_agent
{

   const char *configClassname="org/netxms/agent/Config";
   const char *configEntryClassname="org/netxms/agent/ConfigEntry";

   static bool nativeMethodsRegistered = FALSE;

   // Retrieves pointer to native Config from Java object
   Config *retrieveConfigNativePointer(JNIEnv *env, jobject obj)
   {
      jclass objClass = env->GetObjectClass(obj);
      if (objClass == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not access to the class Config"));
         return NULL;
      }
      jfieldID nativePointerFieldId = env->GetFieldID(objClass, "configHandle", "J");
      if (nativePointerFieldId == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not access to the field Config.configHandle"));
         return NULL;
      }
      jlong result = env->GetLongField(obj, nativePointerFieldId);
      return (Config *) result;
   }

   // Retrieves pointer to native ConfigEntry from Java object
   ConfigEntry *retrieveConfigEntryNativePointer(JNIEnv *env, jobject obj)
   {
      jclass objClass = env->GetObjectClass(obj);
      if (objClass == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not access to the class ConfigEntry"));
         return NULL;
      }
      jfieldID nativePointerFieldId = env->GetFieldID(objClass, "configEntryHandle", "J");
      if (nativePointerFieldId == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not access to the field ConfigEntry.configHandle"));
         return NULL;
      }
      jlong result = env->GetLongField(obj, nativePointerFieldId);
      return (ConfigEntry *) result;
   }

   // Factory - used only by Config and ConfigEntry methods - no need to publish
   jobject createConfigEntryInstance(JNIEnv *curEnv, ConfigEntry *configEntry)
   {
      jmethodID constructObject = NULL ;
      jobject localInstance ;
      jobject instance = NULL;
      jclass instanceClass;
      jclass localClass ;

      const char *construct="<init>";
      const char *param="(J)V";

      localClass = curEnv->FindClass( configEntryClassname ) ;
      if (localClass == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not get the Class %s"), WideStringFromMBString(configEntryClassname));

         throw JNIException();
      }

      instanceClass = static_cast<jclass>(curEnv->NewGlobalRef(localClass));

      /* localClass is not needed anymore */
      curEnv->DeleteLocalRef(localClass);

      if (instanceClass == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not create a Global Ref of %s"), WideStringFromMBString(configEntryClassname));

         throw JNIException();
      }

      constructObject = curEnv->GetMethodID( instanceClass, construct , param ) ;
      if(constructObject == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not retrieve the constructor of the class %s with the profile : %s%s"), WideStringFromMBString(configEntryClassname), WideStringFromMBString(construct), WideStringFromMBString(param));

         throw JNIException();
      }

      localInstance = curEnv->NewObject( instanceClass, constructObject, (jlong) configEntry ) ;
      if(localInstance == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not instantiate the object %s with the constructor : %s%s"), WideStringFromMBString(configEntryClassname), WideStringFromMBString(construct), WideStringFromMBString(param));

         throw JNIException();
      }

      instance = curEnv->NewGlobalRef(localInstance) ;
      if(instance == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not create a new global ref of %s"), WideStringFromMBString(configEntryClassname));

         throw JNIException();
      }
      /* localInstance not needed anymore */
      curEnv->DeleteLocalRef(localInstance);

      return instance;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    lock
    * Signature: ()V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_Config_lock(JNIEnv *jenv, jobject jobj)
   {
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if (config != NULL)
      {
         config->lock();
      }
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    unlock
    * Signature: ()V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_Config_unlock(JNIEnv *jenv, jobject jobj)
   {
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if (config != NULL)
      {
         config->unlock();
      }
   }

   /**
    * Class:     org_netxms_agent_Config
    * Method:    deleteEntry
    * Signature: (Ljava/lang/String;)V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_Config_deleteEntry(JNIEnv *jenv, jobject jobj, jstring jpath)
   {
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if (config == NULL)
      {
         return;
      }
      if (jpath != NULL)
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         config->deleteEntry(path);
         free(path);
      }
   }

   /**
    * Class:     org_netxms_agent_Config
    * Method:    getEntry
    * Signature: (Ljava/lang/String;)Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobject JNICALL Java_org_netxms_agent_Config_getEntry(JNIEnv *jenv, jobject jobj, jstring jpath)
   {
      jobject result = NULL;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if (config == NULL)
      {
         return result;
      }
      if (jpath != NULL)
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         ConfigEntry *configEntry = config->getEntry(path);
         if (configEntry)
         {
            result = createConfigEntryInstance(jenv, configEntry);
         }
         free(path);
      }
      return result;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    getSubEntries
    * Signature: (Ljava/lang/String;Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobjectArray JNICALL Java_org_netxms_agent_Config_getSubEntries(JNIEnv *jenv, jobject jobj, jstring jpath, jstring jmask)
   {
      jobjectArray jresult = NULL;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if (config == NULL)
      {
         return jresult;
      }
      if ((jmask != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         TCHAR *mask = CStringFromJavaString(jenv, jmask);
         ObjectArray<ConfigEntry> *configEntryList = config->getSubEntries(path, mask);
         if (configEntryList != NULL)
         {
            for (int i = 0; i < configEntryList->size(); i++)
            {
               ConfigEntry *configEntry = configEntryList->get(i);
               jobject jconfigEntry = createConfigEntryInstance(jenv, configEntry);
               if (i == 0)
               {
                  jresult = jenv->NewObjectArray((jsize)configEntryList->size(), jenv->GetObjectClass(jconfigEntry), jconfigEntry);
               }
               jenv->SetObjectArrayElement(jresult, (jsize) i, jconfigEntry);
            }
            delete configEntryList;
         }
         free(path);
         free(mask);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    getOrderedSubEntries
    * Signature: (Ljava/lang/String;Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobjectArray JNICALL Java_org_netxms_agent_Config_getOrderedSubEntries(JNIEnv *jenv, jobject jobj, jstring jpath, jstring jmask)
   {
      jobjectArray jresult = NULL;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if (config == NULL)
      {
         return jresult;
      }
      if ((jmask != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         TCHAR *mask = CStringFromJavaString(jenv, jmask);
         ObjectArray<ConfigEntry> *configEntryList = config->getOrderedSubEntries(path, mask);
         for (int i = 0; i < configEntryList->size(); i++)
         {
            ConfigEntry *configEntry = configEntryList->get(i);
            jobject jconfigEntry = createConfigEntryInstance(jenv, configEntry);
            if (i == 0)
            {
               jresult = jenv->NewObjectArray((jsize) configEntryList->size(), jenv->GetObjectClass(jconfigEntry), jconfigEntry);
            }
            jenv->SetObjectArrayElement(jresult, (jsize) i, jconfigEntry);
         }
         free(path);
         free(mask);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    getValue
    * Signature: (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    */
   JNIEXPORT jstring JNICALL Java_org_netxms_agent_Config_getValue(JNIEnv *jenv, jobject jobj, jstring jpath, jstring jvalue)
   {
      jstring jresult = jvalue;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL) && (jvalue != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         TCHAR *value = CStringFromJavaString(jenv, jvalue);
         const TCHAR *result = config->getValue(path, value);
         if (result != NULL)
         {
            jresult = JavaStringFromCString(jenv, result);
         }
         free(path);
         free(value);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    getValueInt
    * Signature: (Ljava/lang/String;I)I
    */
   JNIEXPORT jint JNICALL Java_org_netxms_agent_Config_getValueInt(JNIEnv *jenv, jobject jobj, jstring jpath, jint jvalue)
   {
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         INT32 result = config->getValueAsInt(path, (INT32)jvalue);
         free(path);
         return (jint) result;
      }
      return jvalue;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    getValueLong
    * Signature: (Ljava/lang/String;J)J
    */
   JNIEXPORT jlong JNICALL Java_org_netxms_agent_Config_getValueLong(JNIEnv *jenv, jobject jobj, jstring jpath, jlong jvalue)
   {
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         INT64 result = config->getValueAsInt64(path, (long)jvalue);
         free(path);
         return (jlong)result;
      }
      return jvalue;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    getValueBoolean
    * Signature: (Ljava/lang/String;Z)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_Config_getValueBoolean(JNIEnv *jenv, jobject jobj, jstring jpath, jboolean jvalue)
   {
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         bool result = config->getValueAsBoolean(path, jvalue ? true : false);
         free(path);
         return (jboolean) result;
      }
      return (jboolean) jvalue;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    setValue
    * Signature: (Ljava/lang/String;Ljava/lang/String;)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2Ljava_lang_String_2(JNIEnv *jenv, jobject jobj, jstring jpath, jstring jvalue)
   {
      bool result = FALSE;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL) && (jvalue != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         TCHAR *value = CStringFromJavaString(jenv, jvalue);
         result = config->setValue(path, value);
         free(path);
         free(value);
      }
      return (jboolean) result;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    setValue
    * Signature: (Ljava/lang/String;I)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2I(JNIEnv *jenv, jobject jobj, jstring jpath, jint jvalue)
   {
      bool result = FALSE;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         result = config->setValue(path, (INT32) jvalue);
         free(path);
      }
      return (jboolean) result;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    setValue
    * Signature: (Ljava/lang/String;J)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2J(JNIEnv *jenv, jobject jobj, jstring jpath, jlong jvalue)
   {
      bool result = FALSE;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         result = config->setValue(path, (INT64) jvalue);
         free(path);
      }
      return (jboolean) result;
   }

   /*
    * Class:     org_netxms_agent_Config
    * Method:    setValue
    * Signature: (Ljava/lang/String;D)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2D(JNIEnv *jenv, jobject jobj, jstring jpath, jdouble jvalue)
   {
      bool result = FALSE;
      Config *config = retrieveConfigNativePointer(jenv, jobj);
      if ((config != NULL) && (jpath != NULL))
      {
         TCHAR *path = CStringFromJavaString(jenv, jpath);
         result = config->setValue(path, (double) jvalue);
         free(path);
      }
      return (jboolean) result;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getNext
    * Signature: ()Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobject JNICALL Java_org_netxms_agent_ConfigEntry_getNext(JNIEnv *jenv, jobject jobj)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return NULL;
      }
      return createConfigEntryInstance(jenv, configEntry->getNext());
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getParent
    * Signature: ()Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobject JNICALL Java_org_netxms_agent_ConfigEntry_getParent(JNIEnv *jenv, jobject jobj)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return NULL;
      }
      return createConfigEntryInstance(jenv, configEntry->getParent());
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getName
    * Signature: ()Ljava/lang/String;
    */
   JNIEXPORT jstring JNICALL Java_org_netxms_agent_ConfigEntry_getName(JNIEnv *jenv, jobject jobj)
   {
      jstring jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      const TCHAR *result = configEntry->getName();
      if (result != NULL)
      {
         jresult = JavaStringFromCString(jenv, result);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    setName
    * Signature: (Ljava/lang/String;)V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_ConfigEntry_setName(JNIEnv *jenv, jobject jobj, jstring jname)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return;
      }
      if (jname != NULL)
      {
         TCHAR *name = CStringFromJavaString(jenv, jname);
         configEntry->setName(name);
         free(name);
      }
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getId
    * Signature: ()I
    */
   JNIEXPORT jint JNICALL Java_org_netxms_agent_ConfigEntry_getId(JNIEnv *jenv, jobject jobj)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return 0;
      }
      return (jint) configEntry->getId();
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getValueCount
    * Signature: ()I
    */
   JNIEXPORT jint JNICALL Java_org_netxms_agent_ConfigEntry_getValueCount(JNIEnv *jenv, jobject jobj)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return 0;
      }
      return (jint) configEntry->getValueCount();
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getLine
    * Signature: ()I
    */
   JNIEXPORT jint JNICALL Java_org_netxms_agent_ConfigEntry_getLine(JNIEnv *jenv, jobject jobj)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return 0;
      }
      return (jint) configEntry->getLine();
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    addValue
    * Signature: (Ljava/lang/String;)V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_ConfigEntry_addValue(JNIEnv *jenv, jobject jobj, jstring jvalue)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return;
      }
      if (jvalue != NULL)
      {
         TCHAR *value = CStringFromJavaString(jenv, jvalue);
         configEntry->addValue(value);
         free(value);
      }
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    setValue
    * Signature: (Ljava/lang/String;)V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_ConfigEntry_setValue(JNIEnv *jenv, jobject jobj, jstring jvalue)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return;
      }
      if (jvalue != NULL)
      {
         TCHAR *value = CStringFromJavaString(jenv, jvalue);
         configEntry->setValue(value);
         free(value);
      }
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getFile
    * Signature: ()Ljava/lang/String;
    */
   JNIEXPORT jstring JNICALL Java_org_netxms_agent_ConfigEntry_getFile(JNIEnv *jenv, jobject jobj)
   {
      jstring jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      const TCHAR *result = configEntry->getFile();
      if (result != NULL)
      {
         jresult = JavaStringFromCString(jenv, result);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    createEntry
    * Signature: (Ljava/lang/String;)Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobject JNICALL Java_org_netxms_agent_ConfigEntry_createEntry(JNIEnv *jenv, jobject jobj, jstring jname)
   {
      jobject jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jname != NULL)
      {
         TCHAR *name = CStringFromJavaString(jenv, jname);
         jresult = createConfigEntryInstance(jenv, configEntry->createEntry(name));
         free(name);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    findEntry
    * Signature: (Ljava/lang/String;)Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobject JNICALL Java_org_netxms_agent_ConfigEntry_findEntry(JNIEnv *jenv, jobject jobj, jstring jname)
   {
      jobject jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jname != NULL)
      {
         TCHAR *name = CStringFromJavaString(jenv, jname);
         jresult = createConfigEntryInstance(jenv, configEntry->findEntry(name));
         free(name);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getSubEntries
    * Signature: (Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobjectArray JNICALL Java_org_netxms_agent_ConfigEntry_getSubEntries(JNIEnv *jenv, jobject jobj, jstring jmask)
   {
      jobjectArray jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jmask != NULL)
      {
         TCHAR *mask = CStringFromJavaString(jenv, jmask);
         ObjectArray<ConfigEntry> *configEntryList = configEntry->getSubEntries(mask);
         if (configEntryList != NULL)
         {
            for (int i = 0; i < configEntryList->size(); i++)
            {
               ConfigEntry *subConfigEntry = configEntryList->get(i);
               jobject jconfigEntry = createConfigEntryInstance(jenv, subConfigEntry);
               if (i == 0)
               {
                  jresult = jenv->NewObjectArray((jsize) configEntryList->size(), jenv->GetObjectClass(jconfigEntry), jconfigEntry);
               }
               jenv->SetObjectArrayElement(jresult, (jsize) i, jconfigEntry);
            }
            delete configEntryList;
         }
         free(mask);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getOrderedSubEntries
    * Signature: (Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;
    */
   JNIEXPORT jobjectArray JNICALL Java_org_netxms_agent_ConfigEntry_getOrderedSubEntries(JNIEnv *jenv, jobject jobj, jstring jmask)
   {
      jobjectArray jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jmask != NULL)
      {
         TCHAR *mask = CStringFromJavaString(jenv, jmask);
         ObjectArray<ConfigEntry> *configEntryList = configEntry->getSubEntries(mask);
         if (configEntryList != NULL)
         {
            for(int i = 0; i < configEntryList->size(); i++)
            {
               ConfigEntry *subConfigEntry = configEntryList->get(i);
               jobject jconfigEntry = createConfigEntryInstance(jenv, subConfigEntry);
               if (i == 0)
               {
                  jresult = jenv->NewObjectArray((jsize) configEntryList->size(), jenv->GetObjectClass(jconfigEntry), jconfigEntry);
               }
               jenv->SetObjectArrayElement(jresult, (jsize) i, jconfigEntry);
            }
            delete configEntryList;
         }
         free(mask);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    unlinkEntry
    * Signature: (Lorg/netxms/agent/ConfigEntry;)V
    */
   JNIEXPORT void JNICALL Java_org_netxms_agent_ConfigEntry_unlinkEntry(JNIEnv *jenv, jobject jobj, jobject jentry)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return;
      }
      ConfigEntry *configEntryParam = retrieveConfigEntryNativePointer(jenv, jentry);
      if (configEntryParam == NULL)
      {
         return;
      }
      configEntry->unlinkEntry(configEntryParam);
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getValue
    * Signature: (I)Ljava/lang/String;
    */
   JNIEXPORT jstring JNICALL Java_org_netxms_agent_ConfigEntry_getValue__I(JNIEnv *jenv, jobject jobj, jint jindex)
   {
      jstring jresult = NULL;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      const TCHAR *result = configEntry->getValue((INT32) jindex);
      if (result != NULL)
      {
         jresult = JavaStringFromCString(jenv, result);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getValue
    * Signature: (ILjava/lang/String;)Ljava/lang/String;
    */
   //  JNIEXPORT jstring JNICALL Java_org_netxms_agent_ConfigEntry_getValue__ILjava_lang_String_2(JNIEnv *, jobject, jint, jstring)
   //  {
   //  }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getValueInt
    * Signature: (II)I
    */
   JNIEXPORT jint JNICALL Java_org_netxms_agent_ConfigEntry_getValueInt(JNIEnv *jenv, jobject jobj, jint jindex, jint jdefaultValue)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jdefaultValue;
      }
      return (jint) configEntry->getValueAsInt((INT32) jindex, (INT32) jdefaultValue);
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getValueLong
    * Signature: (IJ)J
    */
   JNIEXPORT jlong JNICALL Java_org_netxms_agent_ConfigEntry_getValueLong(JNIEnv *jenv, jobject jobj, jint jindex, jlong jdefaultValue)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jdefaultValue;
      }
      return (jlong) configEntry->getValueAsInt64((INT32) jindex, (INT64) jdefaultValue);
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getValueBoolean
    * Signature: (IZ)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_ConfigEntry_getValueBoolean(JNIEnv *jenv, jobject jobj, jint jindex, jboolean jdefaultValue)
   {
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jdefaultValue;
      }
      return (jboolean)configEntry->getValueAsBoolean((INT32) jindex, jdefaultValue ? true : false);
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getSubEntryValue
    * Signature: (Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;
    */
   //  JNIEXPORT jstring JNICALL Java_org_netxms_agent_ConfigEntry_getSubEntryValue(JNIEnv *, jobject, jstring, jint, jstring)
   //  {
   //  }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getSubEntryValueInt
    * Signature: (Ljava/lang/String;II)I
    */
   JNIEXPORT jint JNICALL Java_org_netxms_agent_ConfigEntry_getSubEntryValueInt(JNIEnv *jenv, jobject jobj, jstring jname, jint jindex, jint jdefaultValue)
   {
      jint jresult = jdefaultValue;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jname != NULL)
      {
         TCHAR *name = CStringFromJavaString(jenv, jname);
         jresult = (jint) configEntry->getSubEntryValueAsInt(name, (int) jindex, (INT32) jdefaultValue );
         free(name);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getSubEntryValueLong
    * Signature: (Ljava/lang/String;IJ)J
    */
   JNIEXPORT jlong JNICALL Java_org_netxms_agent_ConfigEntry_getSubEntryValueLong(JNIEnv *jenv, jobject jobj, jstring jname, jint jindex, jlong jdefaultValue)
   {
      jlong jresult = jdefaultValue;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jname != NULL)
      {
         TCHAR *name = CStringFromJavaString(jenv, jname);
         jresult = (jlong) configEntry->getSubEntryValueAsInt64(name, (int) jindex, (INT64) jdefaultValue );
         free(name);
      }
      return jresult;
   }

   /*
    * Class:     org_netxms_agent_ConfigEntry
    * Method:    getSubEntryValueBoolean
    * Signature: (Ljava/lang/String;IZ)Z
    */
   JNIEXPORT jboolean JNICALL Java_org_netxms_agent_ConfigEntry_getSubEntryValueBoolean(JNIEnv *jenv, jobject jobj, jstring jname, jint jindex, jboolean jdefaultValue)
   {
      jboolean jresult = jdefaultValue;
      ConfigEntry *configEntry = retrieveConfigEntryNativePointer(jenv, jobj);
      if (configEntry == NULL)
      {
         return jresult;
      }
      if (jname != NULL)
      {
         TCHAR *name = CStringFromJavaString(jenv, jname);
         jresult = (jboolean) configEntry->getSubEntryValueAsBoolean(name, (int)jindex, jdefaultValue ? true : false);
         free(name);
      }
      return jresult;
   }

   static JNINativeMethod jniMethodsConfig[] =
   {
      { "lock", "()V", (void *) Java_org_netxms_agent_Config_lock },
      { "unlock", "()V", (void *) Java_org_netxms_agent_Config_unlock },
      { "deleteEntry", "(Ljava/lang/String;)V", (void *) Java_org_netxms_agent_Config_deleteEntry },
      { "getEntry", "(Ljava/lang/String;)Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_Config_getEntry },
      { "getSubEntries", "(Ljava/lang/String;Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_Config_getSubEntries },
      { "getOrderedSubEntries", "(Ljava/lang/String;Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_Config_getOrderedSubEntries },
      { "getValue", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", (void *) Java_org_netxms_agent_Config_getValue },
      { "getValueInt", "(Ljava/lang/String;I)I", (void *) Java_org_netxms_agent_Config_getValueInt },
      { "getValueLong", "(Ljava/lang/String;J)J", (void *) Java_org_netxms_agent_Config_getValueLong },
      { "getValueBoolean", "(Ljava/lang/String;Z)Z", (void *) Java_org_netxms_agent_Config_getValueBoolean },
      { "setValue", "(Ljava/lang/String;Ljava/lang/String;)Z", (void *) Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2Ljava_lang_String_2 },
      { "setValue", "(Ljava/lang/String;I)Z", (void *) Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2I },
      { "setValue", "(Ljava/lang/String;J)Z", (void *) Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2J },
      { "setValue", "(Ljava/lang/String;D)Z", (void *) Java_org_netxms_agent_Config_setValue__Ljava_lang_String_2D }
   };

   static JNINativeMethod jniMethodsConfigEntry[] =
   {
      { "getNext", "()Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_ConfigEntry_getNext },
      { "getParent", "()Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_ConfigEntry_getParent },
      { "getName", "()Ljava/lang/String;", (void *) Java_org_netxms_agent_ConfigEntry_getName },
      { "setName", "(Ljava/lang/String;)V", (void *) Java_org_netxms_agent_ConfigEntry_setName },
      { "getId", "()I", (void *) Java_org_netxms_agent_ConfigEntry_getId },
      { "getValueCount", "()I", (void *) Java_org_netxms_agent_ConfigEntry_getValueCount },
      { "getLine", "()I", (void *) Java_org_netxms_agent_ConfigEntry_getLine },
      { "addValue", "(Ljava/lang/String;)V", (void *) Java_org_netxms_agent_ConfigEntry_addValue },
      { "setValue", "(Ljava/lang/String;)V", (void *) Java_org_netxms_agent_ConfigEntry_setValue },
      { "getFile", "()Ljava/lang/String;", (void *) Java_org_netxms_agent_ConfigEntry_getFile },
      { "createEntry", "(Ljava/lang/String;)Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_ConfigEntry_createEntry },
      { "findEntry", "(Ljava/lang/String;)Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_ConfigEntry_findEntry },
      { "getSubEntries", "(Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_ConfigEntry_getSubEntries },
      { "getOrderedSubEntries", "(Ljava/lang/String;)[Lorg/netxms/agent/ConfigEntry;", (void *) Java_org_netxms_agent_ConfigEntry_getOrderedSubEntries },
      { "unlinkEntry", "(Lorg/netxms/agent/ConfigEntry;)V", (void *) Java_org_netxms_agent_ConfigEntry_unlinkEntry },
      { "getValue", "(I)Ljava/lang/String;", (void *) Java_org_netxms_agent_ConfigEntry_getValue__I },
      //    { "getValue", "(ILjava/lang/String;)Ljava/lang/String;", (void *) Java_org_netxms_agent_ConfigEntry_getValue__ILjava_lang_String_2 },
      { "getValueInt", "(II)I", (void *) Java_org_netxms_agent_ConfigEntry_getValueInt },
      { "getValueLong", "(IJ)J", (void *) Java_org_netxms_agent_ConfigEntry_getValueLong },
      { "getValueBoolean", "(IZ)Z", (void *) Java_org_netxms_agent_ConfigEntry_getValueBoolean },
      //{ "getSubEntryValue", "(Ljava/lang/String;ILjava/lang/String;)Ljava/lang/String;", (void *) Java_org_netxms_agent_ConfigEntry_getSubEntryValue },
      { "getSubEntryValueInt", "(Ljava/lang/String;II)I", (void *) Java_org_netxms_agent_ConfigEntry_getSubEntryValueInt },
      { "getSubEntryValueLong", "(Ljava/lang/String;IJ)J", (void *) Java_org_netxms_agent_ConfigEntry_getSubEntryValueLong },
      { "getSubEntryValueBoolean", "(Ljava/lang/String;IZ)Z", (void *) Java_org_netxms_agent_ConfigEntry_getSubEntryValueBoolean },
   };

   static bool registerNativeMethods(JNIEnv *curEnv)
   {
      // register native methods exposed by Config
      jclass clazz = curEnv->FindClass( configClassname );
      if (clazz)
      {
         if (curEnv->RegisterNatives(clazz, jniMethodsConfig, (jint) (sizeof (jniMethodsConfig) / sizeof (jniMethodsConfig[0])) ) == 0)
         {
         }
         else
         {
            AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Failed to register native methods for %s"), WideStringFromMBString( configClassname) );
            curEnv->DeleteLocalRef(clazz);
            return false;
         }
         curEnv->DeleteLocalRef(clazz);
      }
      else
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Failed to find main class %s"), WideStringFromMBString( configClassname ));;
         return false;
      }

      // register native methods exposed by ConfigEntry
      clazz = curEnv->FindClass( configEntryClassname );
      if (clazz)
      {
         if (curEnv->RegisterNatives(clazz, jniMethodsConfigEntry, (jint) (sizeof (jniMethodsConfigEntry) / sizeof (jniMethodsConfigEntry[0])) ) == 0)
         {
         }
         else
         {
            AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Failed to register native methods for %s"), WideStringFromMBString( configEntryClassname) );
            curEnv->DeleteLocalRef(clazz);
            return false;
         }
         curEnv->DeleteLocalRef(clazz);
         return true;
      }
      else
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Failed to find main class %s"), WideStringFromMBString( configEntryClassname ));;
         return false;
      }
   }

   // Factory
   jobject ConfigHelper::createInstance(JNIEnv *curEnv, Config *config)
   {
      jmethodID constructObject = NULL ;
      jobject localInstance ;
      jobject instance = NULL;
      jclass instanceClass;
      jclass localClass ;

      const char *construct="<init>";
      const char *param="(J)V";

      if (!nativeMethodsRegistered)
      {
         if (!registerNativeMethods(curEnv))
         {
            return instance;
         }
      }

      localClass = curEnv->FindClass( configClassname ) ;
      if (localClass == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not get the Class %s"), WideStringFromMBString(configClassname));

         throw JNIException();
      }

      instanceClass = static_cast<jclass>(curEnv->NewGlobalRef(localClass));

      /* localClass is not needed anymore */
      curEnv->DeleteLocalRef(localClass);

      if (instanceClass == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not create a Global Ref of %s"), WideStringFromMBString(configClassname));

         throw JNIException();
      }

      constructObject = curEnv->GetMethodID( instanceClass, construct , param ) ;
      if(constructObject == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not retrieve the constructor of the class %s with the profile : %s%s"), WideStringFromMBString(configClassname), WideStringFromMBString(construct), WideStringFromMBString(param));

         throw JNIException();
      }

      localInstance = curEnv->NewObject( instanceClass, constructObject, (jlong) config ) ;
      if(localInstance == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not instantiate the object %s with the constructor : %s%s"), WideStringFromMBString(configClassname), WideStringFromMBString(construct), WideStringFromMBString(param));

         throw JNIException();
      }

      instance = curEnv->NewGlobalRef(localInstance) ;
      if(instance == NULL)
      {
         AgentWriteLog(NXLOG_ERROR, _T("ConfigHelper: Could not create a new global ref of %s"), WideStringFromMBString(configClassname));

         throw JNIException();
      }
      /* localInstance not needed anymore */
      curEnv->DeleteLocalRef(localInstance);

      return instance;
   }

}
