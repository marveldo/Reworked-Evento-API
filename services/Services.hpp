#pragma once
#include "users/UserService.hpp"
#include "devices/DeviceService.hpp"
#include "events/EventService.hpp"
#include "notifications/NotificationService.hpp"
#include "realtime/RealtimeService.hpp"
#include "jwt/JwtService.hpp"
#include "media/CloudinaryService.hpp"
#include "google/GoogleAuthService.hpp"
#include "geo/GeoService.hpp"
#include "settings/Settings.h"
#include "events/EventBus.hpp"
#include <drogon/orm/DbClient.h>

class Services {
    public:
        UserService* userService;
        DeviceService* deviceService;
        EventService* eventService;
        NotificationService* notificationService;
        RealtimeService* realtimeService;
        JwtService* jwtService;
        CloudinaryService* cloudinaryService;
        GoogleAuthService* googleAuthService;
        GeoService* geoService;

    explicit Services(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events) {
        jwtService = new JwtService(
            settings.jwt.accessSecret,
            settings.jwt.refreshSecret,
            settings.jwt.accessExpiration,
            settings.jwt.refreshExpiration
        );
        realtimeService = new RealtimeService(settings.redis.host, settings.redis.port, settings.redis.password,
                                              settings.redis.username, settings.redis.db);
        userService = new UserService(dbClient, events, settings.frontendLink);
        deviceService = new DeviceService(dbClient, events, jwtService);
        eventService = new EventService(dbClient, events, settings.frontendLink);
        notificationService = new NotificationService(dbClient, realtimeService);
        cloudinaryService = new CloudinaryService(
            settings.cloudinary.cloudName,
            settings.cloudinary.apiKey,
            settings.cloudinary.apiSecret,
            settings.cloudinary.apiUrl
        );
        googleAuthService = new GoogleAuthService();
        geoService = new GeoService(settings.geo.ipinfoToken);
    }

    ~Services() {
        delete userService;
        delete deviceService;
        delete eventService;
        delete notificationService;
        delete realtimeService;
        delete jwtService;
        delete cloudinaryService;
        delete googleAuthService;
        delete geoService;
    }
};

Services* getServices(drogon::orm::DbClientPtr dbClient, Settings& settings, EventBus* events);
