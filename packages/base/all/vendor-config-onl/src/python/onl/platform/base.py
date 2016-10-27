#!/usr/bin/python
############################################################
# <bsn.cl fy=2013 v=none>
#
#        Copyright 2013, 2014 BigSwitch Networks, Inc.
#
#
#
# </bsn.cl>
############################################################

import pprint
import json
import os
import re
import yaml
import onl.YamlUtils
import subprocess

class OnlInfoObject(object):
    DEFAULT_INDENT="    "

    def __init__(self, d, klass=None):
        self._data = d
        if klass:
            for (m,n) in klass.__dict__.iteritems():
                if m == m.upper():
                    setattr(self, m, None)

                for (k,v) in d.iteritems():
                    for (m,n) in klass.__dict__.iteritems():
                        if n == k:
                            setattr(self, m, v);
                            break

    def __getattr__(self, name):
        if name in self._data:
            return self._data[name]
        else:
            return None

    def __str__(self, indent=DEFAULT_INDENT):
        """String representation of the information container."""
        return OnlInfoObject.string(self._data, indent)

    @staticmethod
    def string(d, indent=DEFAULT_INDENT):
        return "\n".join( sorted("%s%s: %s" % (indent,k,v) for k,v in d.iteritems() if not k.startswith('_') and d[k] is not None and k != 'CRC'))


############################################################
#
# System-specific information keys.
# These represent information about a particular box.
#
############################################################

class OnieInfo(object):
    PRODUCT_NAME='Product Name'
    PART_NUMBER='Part Number'
    SERIAL_NUMBER='Serial Number'
    MAC_ADDRESS='MAC'
    MAC_RANGE='MAC Range'
    MANUFACTURER='Manufacturer'
    MANUFACTURE_DATE='Manufacture Date'
    VENDOR='Vendor'
    PLATFORM_NAME='Platform Name'
    DEVICE_VERSION='Device Version'
    LABEL_REVISION='Label Revision'
    COUNTRY_CODE='Country Code'
    DIAG_VERSION='Diag Version'
    SERVICE_TAG='Service Tag'
    ONIE_VERSION='ONIE Version'
    oids = {
        PRODUCT_NAME         : '.1',
        PART_NUMBER          : '.2',
        SERIAL_NUMBER        : '.3',
        MAC_ADDRESS          : '.4',
        MAC_RANGE            : '.5',
        MANUFACTURER         : '.6',
        MANUFACTURE_DATE     : '.7',
        VENDOR               : '.8',
        PLATFORM_NAME        : '.9',
        DEVICE_VERSION       : '.10',
        LABEL_REVISION       : '.11',
        COUNTRY_CODE         : '.12',
        DIAG_VERSION         : '.13',
        SERVICE_TAG          : '.14',
        ONIE_VERSION         : '.15',
        }


class PlatformInfo(object):
    CPLD_VERSIONS='CPLD Versions'


############################################################
#
# ONL Platform Base
# Baseclass for all OnlPlatform objects.
#
############################################################
class OnlPlatformBase(object):

    CONFIG_DIR='/lib/platform-config'
    CURRENT_DIR=os.path.join(CONFIG_DIR, 'current')

    CONFIG_DEFAULT_GRUB = "/lib/vendor-config/onl/platform-config-defaults-x86-64.yml"
    CONFIG_DEFAULT_UBOOT = "/lib/vendor-config/onl/platform-config-defaults-uboot.yml"

    def __init__(self):
        self.add_info_json("onie_info", "%s/onie-info.json" % self.basedir_onl(), OnieInfo,
                           required=False)
        self.add_info_json("platform_info", "%s/platform-info.json" % self.basedir_onl(), PlatformInfo,
                           required=False)

        # Find the base platform config
        if self.platform().startswith('x86-64'):
            y1 = self.CONFIG_DEFAULT_GRUB
        elif self.platform().startswith('powerpc'):
            y1 = self.CONFIG_DEFAULT_UBOOT
        elif self.platform().startswith('arm'):
            y1 = self.CONFIG_DEFAULT_UBOOT
        else:
            y1 = None

        # Find and load the platform config yaml file
        y2 = os.path.join(self.basedir_onl(), "%s.yml" % self.platform())
        if os.path.exists(y1) and os.path.exists(y2):
            self.platform_config = onl.YamlUtils.merge(y1, y2)
            if self.platform() in self.platform_config:
                self.platform_config = self.platform_config[self.platform()]
        elif os.path.exists(y2):
            with open(y2) as fd:
                self.platform_config = yaml.load(fd)
            if self.platform() in self.platform_config:
                self.platform_config = self.platform_config[self.platform()]
        elif os.path.exists(y1):
            with open(y1) as fd:
                self.platform_config = yaml.load(fd)
            if 'default' in self.platform_config:
                self.platform_config = self.platform_config['default']
        else:
            self.platform_config = {}


    def add_info_dict(self, name, d, klass=None):
        setattr(self, name, OnlInfoObject(d, klass))

    def add_info_json(self, name, f, klass=None, required=True):
        if os.path.exists(f):
            d = json.load(file(f))
            self.add_info_dict(name, d, klass)
        elif required:
            raise RuntimeError("A required system file (%s) is missing." % f)



    def load_configs(self, reload=False):
        if reload or hasattr(self, 'configs') is False:
            self.configs = {}
            for subsys in os.listdir(self.basedir()):
                cpath = os.path.join(self.basedir(), subsys, "configs")
                if os.path.isdir(cpath):
                    for config in os.listdir(cpath):
                        with file(os.path.join(cpath, config)) as f:
                            if not subsys in self.configs:
                                self.configs[subsys] = {}
                            self.configs[subsys][config] = json.load(f)

    def basedir(self, *args):
        return os.path.join(self.CONFIG_DIR, self.platform(), *args)

    def basedir_onl(self, *args):
        return self.basedir('onl', *args)

    def baseconfig(self):
        return True

    def platform(self):
        return self.PLATFORM

    def baseplatform(self):
        p = self.platform()
        p = re.sub(r'-r\d$', '', p)
        return p

    def description(self):
        return "%s %s" % (self.MANUFACTURER, self.MODEL)

    def serialnumber(self):
        return self.onie_info.SERIAL_NUMBER

    def hw_description(self):
        return "%s (%s)" % (self.onie_info.PRODUCT_NAME,
                            self.onie_info.PART_NUMBER)


    # ONL Platform Information Tree
    def platform_info_oid(self):
        return "1.3.6.1.4.1.42623.1.1"

    # ONL Platform Information General Tree
    def platform_info_general_oid(self):
        return self.platform_info_oid() + ".1"

    # ONL Platform Information General Sys Tree
    def platform_info_general_sys_oid(self):
        return self.platform_info_general_oid() + ".1"

    # ONL Platform Information Vendor Tree
    def platform_info_vendor_oid(self):
        return self.platform_info_oid() + ".2"

    def sys_oid_platform(self):
        raise Exception("sys_oid_platform() is not set.")

    def sys_object_id(self):
        return "%s.%s%s" % (self.platform_info_vendor_oid(), self.PRIVATE_ENTERPRISE_NUMBER, self.SYS_OBJECT_ID)

    def onie_version(self):
        return self.onie_info.ONIE_VERSION

    def firmware_version(self):
        return self.platform_info.CPLD_VERSIONS

    def upgrade_manifest(self, type_, override_dir=None):
        if override_dir:
            m = os.path.join(override_dir, "manifest.json")
        else:
            m = os.path.join(self.basedir_onl(), "upgrade", type_, "manifest.json")

        if os.path.exists(m):
            return (os.path.dirname(m), m, json.load(file(m)))
        else:
            return (None, None, None)


    def new_device(self, driver, addr, bus, devdir):
        if not os.path.exists(os.path.join(bus, devdir)):
            try:
                with open("%s/new_device" % bus, "w") as f:
                    f.write("%s 0x%x\n" % (driver, addr))
            except Exception, e:
                print "Unexpected error initialize device %s:0x%x:%s: %s" % (driver, addr, bus, e)
        else:
            print("Device %s:%x:%s already exists." % (driver, addr, bus))

    def new_devices(self, new_device_list):
        for (driver, addr, bus, devdir) in new_device_list:
            self.new_device(driver, addr, bus, devdir)

    def new_i2c_device(self, driver, addr, bus_number):
        bus = '/sys/bus/i2c/devices/i2c-%d' % bus_number
        devdir = "%d-%4.4x" % (bus_number, addr)
        return self.new_device(driver, addr, bus, devdir)

    def new_i2c_devices(self, new_device_list):
        for (driver, addr, bus_number) in new_device_list:
            self.new_i2c_device(driver, addr, bus_number)

    def ifnumber(self):
        # The default assumption for any platform
        # is ma1 and lo
        return 2

    def environment(self, fmt='user'):
        if fmt not in [ 'user', 'yaml', 'dict', 'json' ]:
            raise ValueError("Unsupported format '%s'" % fmt)

        if fmt == 'user':
            return subprocess.check_output(['/bin/onlpd', '-r'])
        else:
            yamlstr = subprocess.check_output(['/bin/onlpd', '-r', '-y'])
            if fmt == 'yaml':
                return yamlstr
            else:
                data = yaml.load(yamlstr)
                if fmt == 'json':
                    return json.dumps(data, indent=2)
                else:
                    return data

    def __str__(self):
        s = """Model: %s
Manufacturer: %s
Ports: %s (%s)
System Object Id: %s
System Information:
%s
%s
""" % (
            self.MODEL,
            self.MANUFACTURER,
            self.PORT_COUNT,
            self.PORT_CONFIG,
            self.sys_object_id(),
            str(self.onie_info),
            str(self.platform_info),
            )


        if hasattr(self, 'warning'):
            s += """

Warning: %s

""" % (self.warning())
        return s


class OnlPlatformPortConfig_48x1_4x10(object):
    PORT_COUNT=52
    PORT_CONFIG="48x1 + 4x10"

class OnlPlatformPortConfig_48x10_4x40(object):
    PORT_COUNT=52
    PORT_CONFIG="48x10 + 4x40"

class OnlPlatformPortConfig_48x10_6x40(object):
    PORT_COUNT=54
    PORT_CONFIG="48x10 + 6x40"

class OnlPlatformPortConfig_32x40(object):
    PORT_COUNT=32
    PORT_CONFIG="32x40"

class OnlPlatformPortConfig_64x40(object):
    PORT_COUNT=64
    PORT_CONFIG="64x40"

class OnlPlatformPortConfig_32x100(object):
    PORT_COUNT=32
    PORT_CONFIG="32x100"

class OnlPlatformPortConfig_24x1_4x10(object):
    PORT_COUNT=28
    PORT_CONFIG="24x1 + 4x10"

class OnlPlatformPortConfig_8x1_8x10(object):
    PORT_COUNT=16
    PORT_CONFIG="8x1 + 8x10"
