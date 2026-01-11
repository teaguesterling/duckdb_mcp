# MCP Nested Ecosystem Experiment

## Overview
This experiment creates a multi-layered MCP (Model Context Protocol) ecosystem where DuckDB servers can consume MCP resources/tools from other servers, enhance them with SQL views and macros, and then re-expose them through their own MCP interface.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    TOP LAYER: DuckDB Client                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  DuckDB Instance #3                                     │   │
│  │  ┌─────────────┐  Uses MCP Client to connect to:       │   │
│  │  │ SQL Queries │  • Layer 2 DuckDB Server               │   │
│  │  │ & Analysis  │  • Original Python resources           │   │
│  │  └─────────────┘  • Enhanced SQL views/macros           │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────┬───────────────────────────────────────┘
                          │ MCP Protocol (stdio)
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                 MIDDLE LAYER: DuckDB MCP Server                │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  DuckDB Instance #2 (Enhanced Server)                  │   │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────┐   │   │
│  │  │ Base Tables │  │ Python Views │  │ MCP Server  │   │   │
│  │  │ & Queries   │  │ & Macros     │  │ (stdio)     │   │   │
│  │  └─────────────┘  └──────────────┘  └─────────────┘   │   │
│  │        │                 │                           │   │
│  │        │                 │ Uses MCP Client           │   │
│  │        │                 ▼                           │   │
│  │        │        ┌──────────────────┐                 │   │
│  │        │        │ MCP Client       │                 │   │
│  │        │        │ (Python Server)  │                 │   │
│  │        │        └──────────────────┘                 │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────┬───────────────────────────────────────┘
                          │ MCP Protocol (stdio)
                          ▼
┌─────────────────────────────────────────────────────────────────┐
│                  BASE LAYER: Python MCP Server                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Python MCP Server                                      │   │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────┐   │   │
│  │  │ Resources   │  │ Tools        │  │ MCP Server  │   │   │
│  │  │ • datasets  │  │ • query      │  │ (stdio)     │   │   │
│  │  │ • files     │  │ • transform  │  │             │   │   │
│  │  └─────────────┘  └──────────────┘  └─────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

## Components

### 1. Python MCP Server (Base Layer)
- **Purpose**: Provides foundational resources and tools
- **Resources**: Sample datasets, files, configurations
- **Tools**: Basic query, transform, analysis functions
- **Transport**: stdio MCP protocol

### 2. DuckDB Enhanced Server (Middle Layer)
- **Purpose**: Consumes Python resources, creates enhanced SQL views/macros, re-exposes everything
- **Phase A - Consumption**: 
  - Connects to Python MCP server as client
  - Imports/references resources from Python server
  - Creates SQL views and macros that use Python tools
  - Tests functionality
- **Phase B - Serving**:
  - Starts own MCP server (stdio)
  - Exposes both original Python resources AND new SQL constructs
  - Provides unified interface to downstream clients

### 3. DuckDB Client (Top Layer)  
- **Purpose**: End-user interface that can use the full ecosystem
- **Capabilities**:
  - Access original Python server resources
  - Use enhanced SQL views/macros from DuckDB server
  - Perform complex analytics combining both layers
  - Demonstrate the power of layered MCP architecture

## Data Flow

### Phase 1: Foundation Setup
```
Python Server → Basic resources/tools → (running)
```

### Phase 2: Enhancement Layer
```
DuckDB Server → connects to Python → creates views → tests → starts serving
```

### Phase 3: Full Ecosystem
```
DuckDB Client → connects to DuckDB Server → uses enhanced + original resources
```

## Implementation Plan

### Step 1: Create Python MCP Server
```bash
# simple_python_mcp_server.py
# Provides basic resources (sample data) and tools (query, transform)
```

### Step 2: Create DuckDB Enhancement Script  
```bash
# enhance_and_serve.sh
# 1. Connect to Python server via MCP client
# 2. Create views/macros using Python resources
# 3. Test the views/macros work
# 4. Start MCP server exposing everything
```

### Step 3: Create DuckDB Client Script
```bash
# mcp_ecosystem_client.sh  
# 1. Connect to enhanced DuckDB server
# 2. List all available resources (original + enhanced)
# 3. Use both types of resources
# 4. Demonstrate layered capabilities
```

## Expected Outcomes

### Proof of Concepts
1. **MCP Composability**: MCP servers can consume from other MCP servers
2. **SQL Enhancement**: SQL views/macros can wrap MCP tool calls
3. **Resource Layering**: Resources can be enhanced and re-exposed
4. **Ecosystem Scaling**: Complex MCP ecosystems can be built incrementally

### Technical Insights
1. How MCP protocol handles nested server relationships
2. Performance characteristics of layered MCP calls
3. Error handling and debugging in MCP ecosystems
4. Resource lifecycle management across layers

### Potential Applications
1. **Data Pipeline Orchestration**: Chain MCP servers for ETL workflows
2. **API Composition**: Combine multiple data sources via MCP
3. **SQL-as-a-Service**: Expose complex SQL logic via MCP protocol
4. **Federated Analytics**: Distribute computation across MCP nodes

## Files to Create

1. `simple_python_mcp_server.py` - Base layer Python server
2. `enhance_and_serve.sh` - Middle layer DuckDB script  
3. `mcp_ecosystem_client.sh` - Top layer client script
4. `test_ecosystem.sh` - End-to-end test runner
5. `sample_data/` - Test datasets for the experiment

This experiment will demonstrate the true power and composability of the MCP protocol when combined with DuckDB's SQL capabilities!