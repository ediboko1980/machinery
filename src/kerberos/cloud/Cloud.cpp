#include "cloud/Cloud.h"

namespace kerberos
{
    void Cloud::setup(kerberos::StringMap & settings)
    {
        // -------------------------------
        // Upload interval [1.5sec;4.2min]
        
        m_min = 1500;
        m_max = 256000;
        m_interval = m_min;

        startPollThread();
        startUploadThread();
        startWatchThread(settings);
    }
    
    void Cloud::scan()
    {
        
        for(;;)
        {
            std::vector<std::string> storage;
            
            pthread_mutex_lock(&m_cloudLock);
            usleep(m_min*1000);
            helper::getFilesInDirectory(storage, SYMBOL_DIRECTORY); // get all symbol links of directory
            pthread_mutex_unlock(&m_cloudLock);
                
            std::vector<std::string>::iterator it = storage.begin();
            
            while(it != storage.end())
            { 
                std::string file = *it;
                bool hasBeenUploaded = upload(file);
                
                if(hasBeenUploaded)
                {
                    pthread_mutex_lock(&m_cloudLock);
                    unlink(file.c_str()); // remove symbol link
                    pthread_mutex_unlock(&m_cloudLock);
                    m_interval = m_min; // reset interval
                }
                else
                {   
                    m_interval = (m_interval * 2 < m_max) ? m_interval * 2 : m_max; // update interval exponential.
                    break;
                }
                
                it++;
            }
        }
    }
    
    // --------------
    // Upload thread
    
    void * uploadContinuously(void * clo)
    {
        Cloud * cloud = (Cloud *) clo;
        cloud->scan();
    }
    
    void Cloud::startUploadThread()
    {
        pthread_create(&m_uploadThread, NULL, uploadContinuously, this);   
    }
    
    void Cloud::stopUploadThread()
    {
        pthread_cancel(m_uploadThread);
        pthread_join(m_uploadThread, NULL);
    }
    
    // --------------
    // Watch thread
    
    void * watchContinuously(void * file)
    {
        char * fileDirectory = (char *) file;
        Watcher watch;
        watch.setup(fileDirectory);
        watch.scan();
    }
    
    void Cloud::startWatchThread(StringMap & settings)
    {
        const char * file = settings.at("ios.Disk.directory").c_str();
        if(file != 0)
        {
            pthread_create(&m_watchThread, NULL, watchContinuously, (char *) file);
        }
    }
    
    void Cloud::stopWatchThread()
    {
        pthread_cancel(m_watchThread);
        pthread_join(m_watchThread, NULL);
    }

    // --------------
    // Poll hades

    void * pollContinuously(void * clo)
    {
        std::string version = "{\"version\": \"";
        version += VERSION;
        version += "\"}";

        while(true)
        {
            RestClient::post(HADES, "application/json", version);
            usleep(180*1000*1000);
        }
    }

    void Cloud::startPollThread()
    {
        pthread_create(&m_pollThread, NULL, pollContinuously, this);
    }

    void Cloud::stopPollThread()
    {
        pthread_cancel(m_pollThread);
        pthread_join(m_pollThread, NULL);
    }
}