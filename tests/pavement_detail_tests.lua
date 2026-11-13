-- Exercise the generated surface renderer with deterministic geometry.
local function upvalue(fn, wanted)
    for i = 1, 100 do
        local name, value = debug.getupvalue(fn, i)
        if not name then break end
        if name == wanted then return value end
    end
    error('missing renderer upvalue: ' .. wanted)
end
local map = upvalue(a350_render, 'MAPX')
local dl = upvalue(a350_render, 'DL')
local triangles, lines, width = 0, 0, nil
dl.rectf = function() dl.n = dl.n + 6 end
dl.tri = function() triangles = triangles + 1; dl.n = dl.n + 6 end
dl.line = function(_, _, _, _, _, th)
    lines = lines + 1; width = th; dl.n = dl.n + 8
end
local function chunks(n)
    local list = {}
    for i = 1, n do
        list[i] = {minx=0,miny=0,maxx=100,maxy=100,
            t={10,10,30,10,20,30},o={0,0,0,0,0,0}}
    end
    return list
end
local function edges(n)
    local list = {}
    for i = 1, n do
        list[i] = {minx=0,miny=0,maxx=100,maxy=100,xs={10,30},ys={10,30}}
    end
    return list
end
local polygon = {minx=0,miny=0,maxx=100,maxy=100,
    fill_c=chunks(1),fill_m=chunks(2),fill=chunks(3),holes_c={},holes_m={},holes={}}
local geometry = {has_pave=true,polys={polygon},pave_c=edges(1),pave_m=edges(2),pave=edges(3)}
local function draw(quality, range, scale)
    triangles, lines, width = 0, 0, nil
    local c = {G=geometry,XY=function(x,y) return x,y end,sc=scale or 0.5,
        x0=0,y0=0,cw=300,ch=300,vminx=0,vmaxx=100,vminy=0,vmaxy=100,
        detail_quality=quality,rng=range}
    local steps = map.surface_steps(c)
    assert(steps[1]()); assert(steps[2]()); assert(steps[4]())
    return triangles, lines, width, dl.n
end
local expected_levels = {
    {3,3,3,3}, -- ultra
    {3,3,3,3}, -- max
    {3,3,3,3}, -- xhigh
    {2,2,2,3}, -- high
    {1,1,2,3}, -- medium
    {1,1,1,1}, -- low
}
for quality = 0, 5 do
    for index, range in ipairs({2,1,0.2,0.1}) do
        local expected = expected_levels[quality+1][index]
        local t, l = draw(quality, range)
        assert(t == expected and l == expected, 'quality/range geometry mismatch')
    end
end
assert(select(1,draw(nil,0.2)) == 2, 'default must be medium')
assert(select(1,draw(nil,2)) == 1, 'default must use coarse geometry at 2 NM')
-- LOD is independent of display radius/scale. Width retains its old threshold.
for _, scale in ipairs({0.01,0.59,0.6,2}) do
    local t,l,w = draw(4,0.2,scale)
    assert(t == 2 and l == 2)
    assert(w == (scale >= 0.6 and 2 or 1), 'line width changed')
end
-- Stress chunks: fixed fine level, with progressively larger allowances.
polygon.fill = chunks(10000)
geometry.pave = edges(10000)
local xhigh_t,xhigh_l = draw(2,2)
local max_t,max_l = draw(1,2)
local ultra_t,ultra_l = draw(0,2)
assert(max_t == math.floor(xhigh_t * 1.5), 'max fill allowance is not +50%')
assert(max_l > xhigh_l and ultra_l == 10000, 'edge budget / ultra clipping')
assert(ultra_t == 10000, 'ultra fill was budget clipped')
print('PASS pavement quality: 24 range/quality combinations, default, scale independence, unchanged widths, +50% and unlimited budgets')
-- 787 range gates: runway/ILS holding lines at 2 NM and taxi holds at 1 NM.
local function holding_count(range, boeing)
    dl.n, lines = 0, 0
    local g={road={},wline={},dash_y={},tw_net={},tl={},tl_m={},
        hold=edges(1),ils_hold=edges(1),taxi_hold=edges(1)}
    local c={G=g,XY=function(x,y) return x,y end,sc=0.1,x0=0,y0=0,cw=300,ch=300,
        vminx=0,vmaxx=100,vminy=0,vmaxy=100,rng=range,boeing=boeing}
    local steps=map.surface_steps(c)
    assert(steps[7]())
    return lines
end
assert(holding_count(5,true)==0, '787 holding lines must be hidden at 5 NM')
assert(holding_count(2,true)==2, '787 runway and ILS holding lines must show at 2 NM')
assert(holding_count(1,true)==3 and holding_count(0.5,true)==3, '787 taxi holding line must show at 1 and 0.5 NM')
assert(holding_count(2,false)==0, 'other aircraft keep scale-based holding lines')
print('PASS 787 holding-line range gates')
local boeing_labels = upvalue(a350_render, 'boeing_labels')
dl.panel = true
local labels = {}
dl.text = function(_, _, _, name) labels[#labels+1]=name end
local function stand_labels(range)
    labels={}
    local c={rng=range,G={stand_lbls={{x=50,y=50,text='C4'}},lbl={}},vis={},
        x0=0,cw=300,map_bottom=300,vminx=0,vmaxx=100,vminy=0,vmaxy=100,
        XY=function(x,y) return x,y end,placed={},sc=0.1}
    boeing_labels(c)
    return #labels
end
assert(stand_labels(1)==0 and stand_labels(2)==0 and stand_labels(5)==0, '787 stands hidden above 0.5 NM')
assert(stand_labels(0.5)==1 and labels[1]=='C4', '787 stand label visible at 0.5 NM')
print('PASS 787 stand-label range gate')
