#pragma once 
#include "utils/HttpClientPool.hpp"
#include <drogon/utils/coroutine.h>
#include <string>


class PaymentService {
    public :
     explicit PaymentService(std::string apiKey , std::string url);
     drogon::Task<> requestFromclient ();

    
    private:
      std::string apiKey_ ;
      HttpClientPool clients_;
};