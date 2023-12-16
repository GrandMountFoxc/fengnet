local serviceId
local conns = {}

function split(input, delimiter)
    input = tostring(input)
    delimiter = tostring(delimiter)
    if (delimiter == "") then return false end
    local pos, arr = 0, {}
    for st, sp in function() return string.find(input, delimiter, pos, true) end do
        table.insert(arr, string.sub(input, pos, st - 1))
        pos = sp + 1
    end
    table.insert(arr, string.sub(input, pos))
    return arr
end

function OnInit(id)
    print("[lua] gateway OnInit id:"..id)
    serviceId = id
    fengnet.Listen(8888, id)
end

function OnAcceptMsg(listenfd, clientfd)
    print("[lua] chat OnAcceptMsg "..clientfd)
    conns[clientfd] = true
end

function OnSocketData(fd, buff)
    print("[lua] gateway OnSocketData "..fd)
    local getTime = os.date("%c")
    for fd, _ in pairs(conns) do
        print("Send msg to "..fd)
        -- buff = getTime..": "..buff
        fengnet.Write(fd, buff)
    end
end

function OnSocketClose(fd)
    print("[lua] chat OnSocketClose "..fd)
    conns[fd] = nil
end