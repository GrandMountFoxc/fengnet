print("run lua init.lua")

function OnInit(id)
    print("[lua] main Oninit id:"..id)

    -- 聊天室 报错Close 5Resource temporarily unavailabl
    fengnet.NewService("gateway")

    -- local ping1 = fengnet.NewService("ping")
    -- print("[lua] new service ping1:"..ping1)

    -- local ping2 = fengnet.NewService("ping")
    -- print("[lua] new service ping2:"..ping2)
    
    -- local pong = fengnet.NewService("ping")
    -- print("[lua] new service pong:"..pong)
    
    -- -- pingpong结束时会报段错误
    -- fengnet.Send(ping1, pong, "start")
    -- fengnet.Send(ping2, pong, "start")
end

function OnExit()
    print("[lua] main OnExit")
end
