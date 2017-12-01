//  OpenVPN 3 Linux client -- Next generation OpenVPN client
//
//  Copyright (C) 2017      OpenVPN Inc. <sales@openvpn.net>
//  Copyright (C) 2017      David Sommerseth <davids@openvpn.net>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, version 3 of the License
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef OPENVPN3_DBUS_CONFIGMGR_HPP
#define OPENVPN3_DBUS_CONFIGMGR_HPP

#include "common/core-extensions.hpp"
#include "dbus/core.hpp"
#include "dbus/connection-creds.hpp"
#include "dbus/exceptions.hpp"
#include "log/dbus-log.hpp"

using namespace openvpn;

class ConfigManagerSignals : public LogSender
{
public:
    ConfigManagerSignals(GDBusConnection *conn, std::string object_path)
        : LogSender(conn, LogGroup::CONFIGMGR,
                    OpenVPN3DBus_interf_configuration, object_path)
    {
    }

    virtual ~ConfigManagerSignals()
    {
    }

    void LogFATAL(std::string msg)
    {
        Log(log_group, LogCategory::FATAL, msg);
        // FIXME: throw something here, to start shutdown procedures
    }

    void StatusChange(const StatusMajor major, const StatusMinor minor, std::string msg)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, msg.c_str());
        Send("StatusChange", params);
    }

    void StatusChange(const StatusMajor major, const StatusMinor minor)
    {
        GVariant *params = g_variant_new("(uus)", (guint) major, (guint) minor, "");
        Send("StatusChange", params);
    }
};

class ConfigurationAlias : public DBusObject,
                           public ConfigManagerSignals
{
public:
    ConfigurationAlias(GDBusConnection *dbuscon, std::string aliasname, std::string cfgpath)
        : DBusObject(OpenVPN3DBus_rootp_configuration + "/aliases/" + aliasname),
          ConfigManagerSignals(dbuscon, OpenVPN3DBus_rootp_configuration + "/aliases/" + aliasname),
          cfgpath(cfgpath)
    {
        alias = aliasname;
        std::string new_obj_path = OpenVPN3DBus_rootp_configuration + "/aliases/"
                                   + aliasname;

        if (!g_variant_is_object_path(new_obj_path.c_str()))
        {
            THROW_DBUSEXCEPTION("ConfigurationAlias",
                                "Specified alias is invalid");
        }

        std::string introsp_xml ="<node name='" + new_obj_path + "'>"
            "    <interface name='" + OpenVPN3DBus_interf_configuration + "'>"
            "        <property  type='o' name='config_path' access='read'/>"
            "    </interface>"
            "</node>";
        ParseIntrospectionXML(introsp_xml);
    }

    const char * GetAlias()
    {
        return alias.c_str();
    }

    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        THROW_DBUSEXCEPTION("ConfigManagerAlias", "method_call not implemented");
    }


    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        GVariant *ret = NULL;

        if ("config_path" == property_name)
        {
            ret = g_variant_new_string (cfgpath.c_str());
        }
        else
        {
            ret = NULL;
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_FAILED,
                         "Unknown property");
        }
        return ret;
    };


    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        THROW_DBUSEXCEPTION("ConfigManagerAlias", "set property not implemented");
    }


private:
    std::string alias;
    std::string cfgpath;
};

class ConfigurationObject : public DBusObject,
                            public ConfigManagerSignals,
                            public DBusCredentials
{
public:
    ConfigurationObject(GDBusConnection *dbuscon, std::string objpath,
                        uid_t creator, GVariant *params)
        : DBusObject(objpath),
          ConfigManagerSignals(dbuscon, objpath),
          DBusCredentials(dbuscon, creator),
          name(""),
          valid(false),
          readonly(false),
          single_use(false),
          persistent(false),
          alias(nullptr)
    {
        gchar *cfgstr;
        gchar *cfgname_c;
        g_variant_get (params, "(ssbb)",
                       &cfgname_c, &cfgstr,
                       &single_use, &persistent);
        name = std::string(cfgname_c);

        // Parse the options from the imported configuration
        OptionList::Limits limits("profile is too large",
				  ProfileParseLimits::MAX_PROFILE_SIZE,
				  ProfileParseLimits::OPT_OVERHEAD,
				  ProfileParseLimits::TERM_OVERHEAD,
				  ProfileParseLimits::MAX_LINE_SIZE,
				  ProfileParseLimits::MAX_DIRECTIVE_SIZE);
        options.parse_from_config(cfgstr, &limits);

        // FIXME:  Validate the configuration file, ensure --ca/--key/--cert/--dh/--pkcs12
        //         contains files
        valid = true;

        std::string introsp_xml ="<node name='" + objpath + "'>"
            "    <interface name='net.openvpn.v3.configuration'>"
            "        <method name='Fetch'>"
            "            <arg direction='out' type='s' name='config'/>"
            "        </method>"
            "        <method name='FetchJSON'>"
            "            <arg direction='out' type='s' name='config_json'/>"
            "        </method>"
            "        <method name='SetOption'>"
            "            <arg direction='in' type='s' name='option'/>"
            "            <arg direction='in' type='s' name='value'/>"
            "        </method>"
            "        <method name='AccessGrant'>"
            "            <arg direction='in' type='u' name='uid'/>"
            "        </method>"
            "        <method name='AccessRevoke'>"
            "            <arg direction='in' type='u' name='uid'/>"
            "        </method>"
            "        <method name='Seal'/>"
            "        <method name='Remove'/>"
            "        <property type='u' name='owner' access='read'/>"
            "        <property type='au' name='acl' access='read'/>"
            "        <property type='s' name='name' access='read'/>"
            "        <property type='b' name='valid' access='read'/>"
            "        <property type='b' name='readonly' access='read'/>"
            "        <property type='b' name='single_use' access='read'/>"
            "        <property type='b' name='persistent' access='read'/>"
            "        <property type='b' name='public_access' access='readwrite'/>"
            "        <property type='s' name='alias' access='readwrite'/>"
            "    </interface>"
            "</node>";
        ParseIntrospectionXML(introsp_xml);

        g_free(cfgname_c);
        g_free(cfgstr);
    }


    ~ConfigurationObject()
    {
        LogVerb2("Configuration removed");
        IdleCheck_RefDec();
    };

    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        IdleCheck_UpdateTimestamp();
        if ("Fetch" == method_name)
        {
            try
            {
                CheckACL(sender, true);
                g_dbus_method_invocation_return_value(invoc,
                                                      g_variant_new("(s)",
                                                                    options.string_export().c_str()));
                if (single_use)
                {
                    LogVerb2("Single-use configuration fetched");
                    RemoveObject(conn);
                    delete this;
                }
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("FetchJSON" == method_name)
        {
            try
            {
                CheckACL(sender);
                g_dbus_method_invocation_return_value(invoc,
                                                      g_variant_new("(s)",
                                                                    options.json_export().c_str()));

                // Do not remove single-use object with this method.
                // FetchJSON is only used by front-ends, never backends.  So
                // it still needs to be available when the backend calls Fetch.
                //
                // single-use configurations are an automation convenience,
                // not a security feature.  Security is handled via ACLs.
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("SetOption" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error (invoc,
                                                            "net.openvpn.v3.error.ReadOnly",
                                                            "Configuration is sealed and readonly");
                return;
            }
            try
            {
                CheckOwnerAccess(sender);
                // TODO: Implement SetOption
                g_dbus_method_invocation_return_value(invoc, NULL);
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("AccessGrant" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error (invoc,
                                                            "net.openvpn.v3.error.ReadOnly",
                                                            "Configuration is sealed and readonly");
                return;
            }

            try
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                GrantAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogVerb1("Access granted to UID " + std::to_string(uid)
                         + " by UID " + std::to_string(GetUID(sender)));
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("AccessRevoke" == method_name)
        {
            if (readonly)
            {
                g_dbus_method_invocation_return_dbus_error (invoc,
                                                            "net.openvpn.v3.error.ReadOnly",
                                                            "Configuration is sealed and readonly");
                return;
            }

            try
            {
                CheckOwnerAccess(sender);

                uid_t uid = -1;
                g_variant_get(params, "(u)", &uid);
                RevokeAccess(uid);
                g_dbus_method_invocation_return_value(invoc, NULL);

                LogVerb1("Access revoked for UID " + std::to_string(uid)
                         + " by UID " + std::to_string(GetUID(sender)));
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("Seal" == method_name)
        {
            try
            {
                CheckOwnerAccess(sender);

                if (valid) {
                    readonly = true;
                    g_dbus_method_invocation_return_value(invoc, NULL);
                }
                else
                {
                    g_dbus_method_invocation_return_dbus_error (invoc,
                                                                "net.openvpn.v3.error.InvalidData",
                                                                "Configuration is not currently valid");
                }
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
        else if ("Remove" == method_name)
        {
            try
            {
                CheckOwnerAccess(sender);
                RemoveObject(conn);
                g_dbus_method_invocation_return_value(invoc, NULL);
                delete this;
                return;
            }
            catch (DBusCredentialsException& excp)
            {
                LogWarn(excp.err());
                excp.SetDBusError(invoc);
            }
        }
    };


    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        IdleCheck_UpdateTimestamp();
        if ("owner" == property_name)
        {
            return GetOwner();
        }

        try {
            CheckACL(sender);

            GVariant *ret = NULL;

            if ("single_use" == property_name)
            {
                ret = g_variant_new_boolean (single_use);
            }
            else if ("persistent" == property_name)
            {
                ret = g_variant_new_boolean (persistent);
            }
            else if ("valid" == property_name)
            {
                ret = g_variant_new_boolean (valid);
            }
            else if ("readonly" == property_name)
            {
                ret = g_variant_new_boolean (readonly);
            }
            else if ("name"  == property_name)
            {
                ret = g_variant_new_string (name.c_str());
            }
            else if ("alias" == property_name)
            {
                ret = g_variant_new_string(alias ? alias->GetAlias() : "");
            }
            else if ("public_access" == property_name)
            {
                ret = GetPublicAccess();
            }
            else if ("acl" == property_name)
            {
                    ret = GetAccessList();
            }
            else
            {
                g_set_error (error,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "Unknown property");
            }

            return ret;
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
            excp.SetDBusError(error, G_IO_ERROR, G_IO_ERROR_FAILED);
            return NULL;
        }
    };

    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        IdleCheck_UpdateTimestamp();
        if (readonly)
        {
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_READ_ONLY,
                                        obj_path, intf_name, property_name,
                                        "Configuration object is read-only");
        }

        try
        {
            CheckOwnerAccess(sender);

            GVariantBuilder * ret = NULL;
            if (("alias" == property_name) && conn)
            {
                if (nullptr != alias)
                {
                    alias->RemoveObject(conn);
                    delete alias;
                    alias = nullptr;
                }

                try
                {
                    gsize len = 0;
                    alias =  new ConfigurationAlias(conn,
                                                    std::string(g_variant_get_string(value, &len)),
                                                    GetObjectPath());
                    alias->RegisterObject(conn);
                    ret = build_set_property_response(property_name, alias->GetAlias());
                }
                catch (DBusException& err)
                {
                    delete alias;
                    alias = nullptr;
                    throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_EXISTS,
                                                obj_path, intf_name, property_name,
                                                err.getRawError().c_str());
                }
            }
            else if (("public_access" == property_name) && conn)
            {
                bool acl_public = g_variant_get_boolean(value);
                SetPublicAccess(acl_public);
                ret = build_set_property_response(property_name, acl_public);
                LogVerb1("Public access set to "
                         + (acl_public ? std::string("true") : std::string("false"))
                         + " by UID " + std::to_string(GetUID(sender)));
            }
            else
            {
                throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                            obj_path, intf_name, property_name,
                                            "Denied");
            };

            return ret;
        }
        catch (DBusCredentialsException& excp)
        {
            LogWarn(excp.err());
            throw DBusPropertyException(G_IO_ERROR, G_IO_ERROR_FAILED,
                                        obj_path, intf_name, property_name,
                                        excp.getUserError());
        }
    };


private:
    std::string name;
    bool valid;
    bool readonly;
    bool single_use;
    bool persistent;
    ConfigurationAlias *alias;
    OptionListJSON options;
};


class ConfigManagerObject : public DBusObject,
                            public ConfigManagerSignals
{
public:
    ConfigManagerObject(GDBusConnection *dbusc, const std::string objpath)
        : DBusObject(objpath),
          ConfigManagerSignals(dbusc, objpath),
          dbuscon(dbusc),
          creds(dbusc)
    {
        std::stringstream introspection_xml;
        introspection_xml << "<node name='" + objpath + "'>"
                          << "    <interface name='" + OpenVPN3DBus_interf_configuration + "'>"
                          << "        <method name='Import'>"
                          << "          <arg type='s' name='name' direction='in'/>"
                          << "          <arg type='s' name='config_str' direction='in'/>"
                          << "          <arg type='b' name='single_use' direction='in'/>"
                          << "          <arg type='b' name='persistent' direction='in'/>"
                          << "          <arg type='o' name='config_path' direction='out'/>"
                          << "        </method>"
                          << GetLogIntrospection()
                          << "    </interface>"
                          << "</node>";
        ParseIntrospectionXML(introspection_xml);

        Debug("ConfigManagerObject registered on '" + OpenVPN3DBus_interf_configuration + "':" + objpath);
    }

    ~ConfigManagerObject()
    {
        LogInfo("Shutting down");
        RemoveObject(dbuscon);
    }

    void OpenLogFile(std::string filename)
    {
        OpenLogFile(filename);
    }

    void callback_method_call(GDBusConnection *conn,
                              const std::string sender,
                              const std::string obj_path,
                              const std::string intf_name,
                              const std::string method_name,
                              GVariant *params,
                              GDBusMethodInvocation *invoc)
    {
        IdleCheck_UpdateTimestamp();
        if ("Import" == method_name)
        {
            // Import the configuration
            std::string cfgpath = generate_path_uuid(OpenVPN3DBus_rootp_configuration, 'x');

            auto *cfgobj = new ConfigurationObject(dbuscon, cfgpath, creds.GetUID(sender), params);
            IdleCheck_RefInc();
            cfgobj->IdleCheck_Register(IdleCheck_Get());
            cfgobj->RegisterObject(conn);

            Debug(std::string("ConfigurationObject registered on '")
                         + intf_name + "': " + cfgpath
                         + " (owner uid " + std::to_string(creds.GetUID(sender)) + ")");
            g_dbus_method_invocation_return_value(invoc, g_variant_new("(o)", cfgpath.c_str()));
        }
    };


    GVariant * callback_get_property(GDBusConnection *conn,
                                     const std::string sender,
                                     const std::string obj_path,
                                     const std::string intf_name,
                                     const std::string property_name,
                                     GError **error)
    {
        IdleCheck_UpdateTimestamp();
        GVariant *ret = NULL;
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_FAILED,
                     "Unknown property");
        return ret;
    };


    GVariantBuilder * callback_set_property(GDBusConnection *conn,
                                            const std::string sender,
                                            const std::string obj_path,
                                            const std::string intf_name,
                                            const std::string property_name,
                                            GVariant *value,
                                            GError **error)
    {
        THROW_DBUSEXCEPTION("ConfigManagerObject", "get property not implemented");
    }


private:
    GDBusConnection *dbuscon;
    DBusConnectionCreds creds;
};


class ConfigManagerDBus : public DBus
{
public:
    ConfigManagerDBus(GBusType bustype)
        : DBus(bustype,
               OpenVPN3DBus_name_configuration,
               OpenVPN3DBus_rootp_configuration,
               OpenVPN3DBus_interf_configuration),
          cfgmgr(nullptr),
          procsig(nullptr),
          logfile("")
    {
    };

    ~ConfigManagerDBus()
    {
        delete cfgmgr;

        procsig->ProcessChange(StatusMinor::PROC_STOPPED);
        delete procsig;
    }

    void SetLogFile(std::string filename)
    {
        logfile = filename;
    }

    void callback_bus_acquired()
    {
        cfgmgr = new ConfigManagerObject(GetConnection(), GetRootPath());
        if (!logfile.empty())
        {
            cfgmgr->OpenLogFile(logfile);
        }
        cfgmgr->RegisterObject(GetConnection());

        procsig = new ProcessSignalProducer(GetConnection(),
                                            OpenVPN3DBus_interf_configuration,
                                            "ConfigurationManager");
        procsig->ProcessChange(StatusMinor::PROC_STARTED);

        if (nullptr != idle_checker)
        {
            cfgmgr->IdleCheck_Register(idle_checker);
        }
    };

    void callback_name_acquired(GDBusConnection *conn, std::string busname)
    {
    };

    void callback_name_lost(GDBusConnection *conn, std::string busname)
    {
        THROW_DBUSEXCEPTION("ConfigManagerDBus", "Configuration D-Bus name not registered: '" + busname + "'");
    };

private:
    ConfigManagerObject * cfgmgr;
    ProcessSignalProducer * procsig;
    std::string logfile;
};

#endif // OPENVPN3_DBUS_CONFIGMGR_HPP
