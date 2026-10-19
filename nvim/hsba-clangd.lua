--[[
    HsBaSlicer — Neovim clangd + clang-tidy 配置
    ================================================
    将此文件放入 ~/.config/nvim/after/lsp/clangd.lua（Neovim 0.10+ 自动加载）
    或在 init.lua 中 require("nvim.hsba-clangd") 手动引用。

    依赖:
      - nvim-lspconfig  (或直接使用 vim.lsp.config, Neovim 0.11+)
      - conform.nvim    (clang-format)
      - nvim-lint       (clang-tidy 独立 lint, 可选)
      - mason.nvim      (可选, 自动安装 clangd/clang-tidy)

    compile_commands.json 准备:
      cmake --preset x64-debug          # Windows
      cmake --preset linux-debug        # Linux
      ln -sf out/build/linux-debug/compile_commands.json .   # 链接到根目录
--]]

local M = {}

-- ============================================================================
-- clangd LSP 配置
-- ============================================================================

---查找 compile_commands.json（项目根 → build 子目录 → .clangd 声明）
---@return string|nil compile_commands.json 所在目录
local function find_compile_db()
    local root = vim.fs.root(vim.fn.getcwd(), { ".clangd", "CMakeLists.txt", ".git" })
    if not root then return nil end

    -- 优先检查根目录（符号链接方案）
    if vim.uv.fs_stat(root .. "/compile_commands.json") then
        return root
    end

    -- 回退到预设构建目录（按平台检测）
    local candidates = {
        "out/build/x64-debug",
        "out/build/linux-debug",
        "out/build/macos-debug",
        "out/build",
        "build",
    }
    for _, dir in ipairs(candidates) do
        if vim.uv.fs_stat(root .. "/" .. dir .. "/compile_commands.json") then
            return root .. "/" .. dir
        end
    end
    return nil
end

---平台检测 query-driver 路径
---@return string[]
local function platform_args()
    local args = {
        "--background-index",
        "--clang-tidy",
        "--clang-tidy-checks=clang-analyzer-*,bugprone-*,performance-*,modernize-*,readability-*",
        "--header-insertion=never",
        "--completion-style=detailed",
        "--function-arg-placeholders",
        "--fallback-style=none",
        "-j=8",
        "--pch-storage=memory",
    }

    -- Windows: MSVC 需要 query-driver 提取 include 路径
    if vim.uv.os_uname().sysname == "Windows_NT" then
        -- vswhere 定位 MSVC 安装路径
        local vswhere = os.getenv("ProgramFiles(x86)") .. "\\Microsoft Visual Studio\\Installer\\vswhere.exe"
        local handle = io.popen('"' .. vswhere .. ' -latest -property installationPath 2>nul')
        if handle then
            local vs_path = handle:read("*a"):gsub("%s+$", "")
            handle:close()
            if vs_path ~= "" then
                local msvc_glob = vs_path .. "\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\cl.exe"
                table.insert(args, "--query-driver=" .. msvc_glob)
            end
        end
        -- fallback: clang-cl
        table.insert(args, "--query-driver=**/clang-cl.exe")
    end

    return args
end

---clangd LSP 配置表（兼容 nvim-lspconfig 和 Neovim 0.11 vim.lsp.config）
M.clangd = {
    cmd = { "clangd", unpack(platform_args()) },
    filetypes = { "c", "cpp", "objc", "objcpp", "cuda" },
    root_markers = { ".clangd", "compile_commands.json", "CMakeLists.txt", ".git" },
    init_options = {
        compilationDatabaseCompileCommands = nil, -- 自动检测
        compilationDatabasePath = find_compile_db(),
        clangdFallback = "cmake",
    },
    settings = {
        clangd = {
            InlayHints = {
                Enabled = true,
                ParameterNames = true,
                DeducedTypes = true,
            },
            Hover = { ShowAKA = true },
        },
    },
    handlers = {
        -- 文本格式化: 使用 clangd 内置 clang-format（读取 .clang-format）
        ["textDocument/formatting"] = vim.lsp.with(vim.lsp.handlers.formatting),
    },
    on_attach = function(client, bufnr)
        -- 键位映射
        local opts = { buffer = bufnr, noremap = true, silent = true }
        vim.keymap.set("n", "gd", vim.lsp.buf.definition, opts)
        vim.keymap.set("n", "gD", vim.lsp.buf.declaration, opts)
        vim.keymap.set("n", "gr", vim.lsp.buf.references, opts)
        vim.keymap.set("n", "gi", vim.lsp.buf.implementation, opts)
        vim.keymap.set("n", "K", vim.lsp.buf.hover, opts)
        vim.keymap.set("n", "<leader>rn", vim.lsp.buf.rename, opts)
        vim.keymap.set("v", "<leader>ca", vim.lsp.buf.code_action, opts)
        vim.keymap.set("n", "<leader>f", function() vim.lsp.buf.format({ async = true }) end, opts)
        vim.keymap.set("n", "<leader>d", vim.diagnostic.open_float, opts)

        -- clangd 专属: 切换头文件/源文件 (Ctrl-Tab)
        vim.keymap.set("n", "<c-Tab>", function()
            client:exec_cmd({
                title = "switch_source_header",
                command = "clangd.switchsourceheader",
            }, { bufnr = bufnr })
        end, opts)
    end,
}

-- ============================================================================
-- Neovim 0.10+ 自动加载格式 (放入 after/lsp/clangd.lua 时使用)
-- ============================================================================
-- 如果不使用此模块函数，可直接将以下内容保存为 after/lsp/clangd.lua:
--
--   return {
--       cmd = { "clangd",
--           "--background-index", "--clang-tidy",
--           "--header-insertion=never",
--           "--completion-style=detailed",
--           "--fallback-style=none",
--           "-j=8", "--pch-storage=memory",
--       },
--       filetypes = { "c", "cpp", "objc", "objcpp", "cuda" },
--       root_markers = { ".clangd", "compile_commands.json", ".git" },
--   }
-- ============================================================================

-- ============================================================================
-- conform.nvim — clang-format 格式化引擎
-- ============================================================================
M.conform = {
    formatters_by_ft = {
        c = { "clang_format" },
        cpp = { "clang_format" },
        lua = { "stylua" },
    },
    formatters = {
        clang_format = {
            -- 自动查找 .clang-format（项目根已有）
            -- 如果 clangd 已处理格式化，此处可作为 fallback
        },
    },
    format_on_save = {
        timeout_ms = 500,
        lsp_format = "fallback", -- 优先用 LSP (clangd), fallback 到 conform
    },
}

-- ============================================================================
-- nvim-lint — clang-tidy 独立 lint (可选, 若 clangd --clang-tidy 不够)
-- ============================================================================
M.lint = {
    c = { "clang_tidy" },
    cpp = { "clang_tidy" },
}

-- ============================================================================
-- 一键安装 (使用 mason.nvim 时)
-- ============================================================================
-- :MasonInstall clangd clang-tidy clang-format

return M
