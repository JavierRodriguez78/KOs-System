#ifndef KOS_SERVICES_USER_SERVICE_HPP
#define KOS_SERVICES_USER_SERVICE_HPP

#include <services/service.hpp>
#include <common/types.hpp>

using namespace kos::common;

namespace kos {
namespace services {

    struct User {
        uint32_t uid;
        int8_t   name[32];
        uint64_t pass_hash; // salted hash
        uint64_t salt;      // per-user salt
        uint32_t gid;
        bool     locked;
    };

    struct Group {
        uint32_t gid;
        int8_t   name[32];
    };

    class UserService : public IService {
    public:
        virtual const char* Name() const { return "USER"; }
        virtual bool Start();
        virtual void Stop() {}
        virtual bool DefaultEnabled() const { return true; }

        // User management
        bool AddUser(const int8_t* name, const int8_t* password, bool superuser=false);
        bool DelUser(const int8_t* name);
        bool SetPassword(const int8_t* name, const int8_t* newPassword);

        // Authentication and session
        bool Authenticate(const int8_t* name, const int8_t* password);
        bool Login(const int8_t* name, const int8_t* password);
        void Logout();

        const User* CurrentUser() const;
        const int8_t* CurrentUserName() const;
        bool IsSuperuser() const;

        // Query
        int32_t UserCount() const;
        const User* Find(const int8_t* name) const;

    private:
        static const int32_t kMaxUsers = 16;
        User      m_users[kMaxUsers];
        int32_t   m_user_count = 0;
        int32_t   m_current_index = -1;

        uint64_t HashPassword(const int8_t* s, uint64_t salt) const;
        int32_t  IndexOf(const int8_t* name) const;
        bool     ValidateUsername(const int8_t* name) const;
    };

    // Global accessor to the singleton user service instance
    UserService* GetUserService();

} // namespace services
} // namespace kos

#endif // KOS_SERVICES_USER_SERVICE_HPP
