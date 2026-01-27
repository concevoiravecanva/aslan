-- Aslan Formula Evaluation
-- Called by the engine when row data changes.

-- Function to evaluate a formula string with a given context (row data)
-- @param context: Table where keys are column names (e.g., { Price = 10, Qty = 2 })
-- @param formula: String expression (e.g., "return Price * Qty")
-- @return: Number result or Error string
function eval(context, formula)
    -- Sandbox environment: inherit from context
    local env = context or {}
    
    -- Allow standard math functions
    env.math = math
    env.abs = math.abs
    env.max = math.max
    env.min = math.min
    env.floor = math.floor
    env.ceil = math.ceil
    
    -- Load the chunk
    -- If formula doesn't start with "return", wrap it? 
    -- User said "col_A + col_B", so it's an expression.
    -- We can prepend "return " if it looks like an expression.
    if not formula:match("^return") then
        formula = "return " .. formula
    end

    local chunk, err = load(formula, "formula", "t", env)
    if not chunk then
        return nil, "Parse Error: " .. tostring(err)
    end

    -- execution with pcall to catch runtime errors
    local ok, result = pcall(chunk)
    if not ok then
        return nil, "Run Error: " .. tostring(result)
    end

    return result
end
