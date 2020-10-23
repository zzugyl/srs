/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2020 Winlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <srs_app_security.hpp>

#include <srs_kernel_log.hpp>
#include <srs_kernel_error.hpp>
#include <srs_app_config.hpp>

using namespace std;

SrsSecurity::SrsSecurity()
{
}

SrsSecurity::~SrsSecurity()
{
}

srs_error_t SrsSecurity::check(SrsRtmpConnType type, string ip, SrsRequest* req)
{
    srs_error_t err = srs_success;

    // allow all if security disabled.
    if (!_srs_config->get_security_enabled(req->vhost)) {
        return err; // OK
    }

    // rules to apply
    SrsConfDirective* rules = _srs_config->get_security_rules(req->vhost);
    return do_check(rules, type, ip, req);
}

srs_error_t SrsSecurity::do_check(SrsConfDirective* rules, SrsRtmpConnType type, string ip, SrsRequest* req)
{
    srs_error_t err = srs_success;
    srs_error_t deny_err = srs_success;
    srs_error_t allow_err = srs_success;

    if (!rules) {
        return srs_error_new(ERROR_SYSTEM_SECURITY, "default deny for %s", ip.c_str());
    }

    // deny if matches deny strategy.
    if ((deny_err = deny_check(rules, type, ip)) != srs_success) {
        err = srs_error_wrap(err, "for %s", ip.c_str());
    }
    
    // allow if matches allow strategy.
    if ((allow_err = allow_check(rules, type, ip)) != srs_success) {
        err = srs_error_wrap(err, "for %s", ip.c_str());
    }

    // if deny is not success but allow is, allow overrides it
    if (deny_err != srs_success && allow_err == srs_success) {
        srs_trace("allowing ip=%s because allow rule has precedence over deny", ip.c_str(), type);
        err = srs_success;
    }

    return err;
}

srs_error_t SrsSecurity::allow_check(SrsConfDirective* rules, SrsRtmpConnType type, std::string ip)
{
    int allow_rules = 0;
    int deny_rules = 0;

    for (int i = 0; i < (int)rules->directives.size(); i++) {
        SrsConfDirective* rule = rules->at(i);

        if (rule->name != "allow") {
            if (rule->name == "deny") {
                deny_rules++;
            }
            continue;
        }
        allow_rules++;

        string cidr_ipv4 = srs_get_cidr_ipv4(rule->arg1());
        string cidr_mask = srs_get_cidr_mask(rule->arg1());
        bool is_ipv4 = srs_is_ipv4(cidr_ipv4);
        bool is_cidr_mask = cidr_mask != "";
        bool within_mask = srs_ipv4_within_mask(ip, cidr_ipv4, cidr_mask);

        switch (type) {
            case SrsRtmpConnPlay:
                if (rule->arg0() != "play") {
                    break;
                }

                srs_trace("attempting to play with ip=%s (ipv4?=%d) for allow rule ip=%s (ipv4?=%d) mask=%s -> within?=%d",
                            ip.c_str(), srs_is_ipv4(ip), cidr_ipv4.c_str(), is_ipv4, cidr_mask.c_str(), within_mask);

                if (rule->arg1() == "all" || rule->arg1() == ip) {
                    return srs_success; // OK
                }
                if (is_ipv4 && is_cidr_mask && within_mask) {
                    return srs_success; // OK
                }
                break;
            case SrsRtmpConnFMLEPublish:
            case SrsRtmpConnFlashPublish:
            case SrsRtmpConnHaivisionPublish:
                if (rule->arg0() != "publish") {
                    break;
                }

                srs_trace("attempting to publish with ip=%s (ipv4?=%d) for allow rule ip=%s (ipv4?=%d) mask=%s -> within?=%d",
                            ip.c_str(), srs_is_ipv4(ip), cidr_ipv4.c_str(), is_ipv4, cidr_mask.c_str(), within_mask);

                if (rule->arg1() == "all" || rule->arg1() == ip) {
                    return srs_success; // OK
                }
                if (is_ipv4 && is_cidr_mask && within_mask) {
                    return srs_success; // OK
                }
                break;
            case SrsRtmpConnUnknown:
            default:
                break;
        }
    }

    if (allow_rules > 0 || (deny_rules + allow_rules) == 0) {
        return srs_error_new(ERROR_SYSTEM_SECURITY_ALLOW, "not allowed by any of %d/%d rules", allow_rules, deny_rules);
    }
    return srs_success; // OK
}

srs_error_t SrsSecurity::deny_check(SrsConfDirective* rules, SrsRtmpConnType type, std::string ip)
{
    for (int i = 0; i < (int)rules->directives.size(); i++) {
        SrsConfDirective* rule = rules->at(i);
        
        if (rule->name != "deny") {
            continue;
        }

        string cidr_ipv4 = srs_get_cidr_ipv4(rule->arg1());
        string cidr_mask = srs_get_cidr_mask(rule->arg1());
        bool is_ipv4 = srs_is_ipv4(cidr_ipv4);
        bool is_cidr_mask = cidr_mask != "";
        bool within_mask = srs_ipv4_within_mask(ip, cidr_ipv4, cidr_mask);
        
        switch (type) {
            case SrsRtmpConnPlay:
                if (rule->arg0() != "play") {
                    break;
                }

                srs_trace("attempting to play with ip=%s (ipv4?=%d) for deny rule ip=%s (ipv4?=%d) mask=%s -> within?=%d",
                            ip.c_str(), srs_is_ipv4(ip), cidr_ipv4.c_str(), is_ipv4, cidr_mask.c_str(), within_mask);

                if (rule->arg1() == "all" || rule->arg1() == ip) {
                    return srs_error_new(ERROR_SYSTEM_SECURITY_DENY, "deny by rule<%s>", rule->arg1().c_str());
                }
                if (is_ipv4 && is_cidr_mask && within_mask) {
                    return srs_error_new(ERROR_SYSTEM_SECURITY_DENY, "deny by rule<%s>", rule->arg1().c_str());
                }
                break;
            case SrsRtmpConnFMLEPublish:
            case SrsRtmpConnFlashPublish:
            case SrsRtmpConnHaivisionPublish:
                if (rule->arg0() != "publish") {
                    break;
                }

                srs_trace("attempting to publish with ip=%s (ipv4?=%d) for deny rule ip=%s (ipv4?=%d) mask=%s -> within?=%d",
                            ip.c_str(), srs_is_ipv4(ip), cidr_ipv4.c_str(), is_ipv4, cidr_mask.c_str(), within_mask);

                if (rule->arg1() == "all" || rule->arg1() == ip) {
                    return srs_error_new(ERROR_SYSTEM_SECURITY_DENY, "deny by rule<%s>", rule->arg1().c_str());
                }
                if (is_ipv4 && is_cidr_mask && within_mask) {
                    return srs_error_new(ERROR_SYSTEM_SECURITY_DENY, "deny by rule<%s>", rule->arg1().c_str());
                }
                break;
            case SrsRtmpConnUnknown:
            default:
                break;
        }
    }
    
    return srs_success; // OK
}

