-- simple custom fill script for tests
-- returns an array of polylines (each polyline is array of {x=.., y=..})

local w = 10000
local margin = 1000

function generate_fill(poly)
    -- Two diagonal polylines across the square
    return {
        { { x = margin, y = margin }, { x = w - margin, y = w - margin } },
        { { x = margin, y = w - margin }, { x = w - margin, y = margin } }
    }
end

return { generate_fill = generate_fill }
