#pragma once
#include <string>

inline const std::string USER_CREATED         = "user.created";
inline const std::string DEVICE_LOGGED_IN     = "device.logged_in";
inline const std::string DEVICE_LOGGED_OUT    = "device.logged_out";
inline const std::string ATTENDEE_REGISTERED  = "event.attendee_registered";
inline const std::string PHOTO_UNUSED         = "photo.unused";
inline const std::string PAYMENT_RECEIVED     = "payment.received";

struct UserCreated {
    std::string id;
    std::string email;
    std::string fullName;
};

struct DeviceLoggedIn {
    std::string deviceId;
    std::string userId;
    std::string ipAddress;
};

struct DeviceLoggedOut {
    std::string deviceId;
};

struct AttendeeRegistered {
    std::string eventId;
    std::string hostId;
    std::string attendeeEmail;
};

struct PhotoUnused {
    std::string publicId;
};

struct PaymentReceived {
    std::string reference;
};
