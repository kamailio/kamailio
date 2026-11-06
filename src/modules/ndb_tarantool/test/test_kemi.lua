function ksr_request_route()
    KSR.info("===============================================================\n")
    KSR.info("       ndb_tarantool KEMI LUA REGRESSION SUITE START           \n")
    KSR.info("===============================================================\n")

    -- 1. KSR.tarantool.eval
    local rc = KSR.tarantool.eval("return 777", "[]", "$var(kemi_res1)")
    local val1 = KSR.pv.get("$var(kemi_res1)")
    if rc <= 0 or val1 ~= "[777]" then
        KSR.err(">>> [KEMI 1 FAILED] eval returned: " .. tostring(val1) .. " <<<\n")
        KSR.sl.send_reply(500, "KEMI 1 Failed")
        return 1
    end
    KSR.info(">>> [KEMI 1 PASSED] eval returned: " .. tostring(val1) .. " <<<\n")

    -- 2. KSR.tarantool.call
    rc = KSR.tarantool.call("ttl_get_stats", "[]", "$var(kemi_res2)")
    local val2 = KSR.pv.get("$var(kemi_res2)")
    if rc <= 0 or not string.find(tostring(val2), "sweeps_count") then
        KSR.err(">>> [KEMI 2 FAILED] call returned: " .. tostring(val2) .. " <<<\n")
        KSR.sl.send_reply(500, "KEMI 2 Failed")
        return 1
    end
    KSR.info(">>> [KEMI 2 PASSED] call ttl_get_stats: " .. tostring(val2) .. " <<<\n")

    -- 3. KSR.tarantool.eval_srv
    rc = KSR.tarantool.eval_srv("default", "return 'KEMI_EVAL_SRV_OK'", "[]", "$var(kemi_res3)")
    local val3 = KSR.pv.get("$var(kemi_res3)")
    if rc <= 0 or not string.find(tostring(val3), "KEMI_EVAL_SRV_OK") then
        KSR.err(">>> [KEMI 3 FAILED] eval_srv returned: " .. tostring(val3) .. " <<<\n")
        KSR.sl.send_reply(500, "KEMI 3 Failed")
        return 1
    end
    KSR.info(">>> [KEMI 3 PASSED] eval_srv returned: " .. tostring(val3) .. " <<<\n")

    -- 4. KSR.tarantool.call_srv
    rc = KSR.tarantool.call_srv("default", "ttl_get_stats", "[]", "$var(kemi_res4)")
    local val4 = KSR.pv.get("$var(kemi_res4)")
    if rc <= 0 or not string.find(tostring(val4), "sweeps_count") then
        KSR.err(">>> [KEMI 4 FAILED] call_srv returned: " .. tostring(val4) .. " <<<\n")
        KSR.sl.send_reply(500, "KEMI 4 Failed")
        return 1
    end
    KSR.info(">>> [KEMI 4 PASSED] call_srv ttl_get_stats: " .. tostring(val4) .. " <<<\n")

    KSR.info("===============================================================\n")
    KSR.info("       ALL 4 ndb_tarantool KEMI TESTS PASSED                   \n")
    KSR.info("===============================================================\n")
    KSR.sl.send_reply(200, "OK - All 4 ndb_tarantool KEMI tests passed")
    return 1
end
