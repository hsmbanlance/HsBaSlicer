--simple image from polygons script for tests
--returns bytes table
local w = 10000
local h = 10000

function generate_image(polys)
    -- create blank white image
    local img = {}
    for i = 1, w * h do
        img[i] = 255
    end
    -- draw black polygons
    for _, poly in ipairs(polys) do
        -- very simple polygon fill: just draw the outline in black
        for i = 1, #poly do
            local p1 = poly[i]
            local p2 = poly[(i % #poly) + 1]
            -- Bresenham's line algorithm
            local x1 = math.floor(p1.x)
            local y1 = math.floor(p1.y)
            local x2 = math.floor(p2.x)
            local y2 = math.floor(p2.y)
            local dx = math.abs(x2 - x1)
            local dy = math.abs(y2 - y1)
            local sx = x1 < x2 and 1 or -1
            local sy = y1 < y2 and 1 or -1
            local err = dx - dy
            while true do
                if x1 >= 0 and x1 < w and y1 >= 0 and y1 < h then
                    img[y1 * w + x1 + 1] = 0
                end
                if x1 == x2 and y1 == y2 then break end
                local e2 = err * 2
                if e2 > -dy then
                    err = err - dy
                    x1 = x1 + sx
                end
                if e2 < dx then
                    err = err + dx
                    y1 = y1 + sy
                end
            end
        end
    end
    return img
    
end