#include "RegisterEvents.hpp"
#include "Events.hpp"

void RegisterEvents(EventBus* bus, TaskQueue* queue) {
    bus->Subscribe<UserCreated>(USER_CREATED, [queue](UserCreated event) {
        queue->enqueue("send_email", {
            {"to", event.email},
            {"subject", "Welcome To evento"},
            {"template", "welcome.html"},
            {"data", {{"full_name", event.fullName}}}
        });
    });

    bus->Subscribe<DeviceLoggedIn>(DEVICE_LOGGED_IN, [queue](DeviceLoggedIn event) {
        queue->enqueue("update_user_location", {{"user_id", event.userId}, {"ip_address", event.ipAddress}});
    });

    bus->Subscribe<DeviceLoggedIn>(DEVICE_LOGGED_IN, [queue](DeviceLoggedIn details) {
        queue->enqueue("create_notification", {
            {"user_id", details.userId},
            {"message", "device with IP " + details.ipAddress + " Just Logged into Your Account"}
        });
    });

    bus->Subscribe<DeviceLoggedOut>(DEVICE_LOGGED_OUT, [queue](DeviceLoggedOut event) {
        queue->enqueue("send_logout", {{"device_id", event.deviceId}});
    });

    bus->Subscribe<AttendeeRegistered>(ATTENDEE_REGISTERED, [queue](AttendeeRegistered event) {
        queue->enqueue("create_notification", {
            {"user_id", event.hostId},
            {"message", event.attendeeEmail + " just registered for your event"}
        });
    });

    bus->Subscribe<PhotoUnused>(PHOTO_UNUSED, [queue](PhotoUnused event) {
        queue->enqueue("delete_photo", {{"public_id", event.publicId}});
    });

    bus->Subscribe<PaymentReceived>(PAYMENT_RECEIVED, [queue](PaymentReceived event) {
        queue->enqueue("settle_payment", {{"reference", event.reference}});
    });
}
