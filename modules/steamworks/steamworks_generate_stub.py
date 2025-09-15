import json


def generate_type_map(typedefs):
    td_map = {}
    for td in typedefs:
        td_map[td["typedef"]] = td["type"]
    return td_map


def write_struct_methods(f, typedef_map, structs):
    for struct in structs:
        if "methods" not in struct:
            continue
        for method in struct["methods"]:
            if "methodname_flat" not in method:
                continue
            return_type = method["returntype_flat"] if "returntype_flat" in method else method["returntype"]
            # Write signature
            f.write(f"S_API {return_type} {method['methodname_flat']}(")

            # Write params
            # self as interface first
            f.write(f"{struct['struct']} * self")
            if len(method["params"]) > 0:
                f.write(", ")
            for i, param in enumerate(method["params"]):
                param_type = param["paramtype_flat"] if "paramtype_flat" in param else param["paramtype"]
                f.write(f"{param_type} {param['paramname']}")
                if i < len(method["params"]) - 1:
                    f.write(", ")

            f.write(") {\n")
            typedef_return_type = typedef_map.get(return_type, return_type)
            match typedef_return_type:
                case "void":
                    pass
                case "float":
                    f.write("\treturn 0.0f;\n")
                case (
                    "unsigned short"
                    | "short"
                    | "int"
                    | "long long"
                    | "unsigned long long"
                    | "unsigned int"
                    | "unsigned char"
                    | "uint64_steamid"
                ):
                    f.write("\treturn 0;\n")
                case "bool":
                    f.write("\treturn false;\n")
                case _:
                    if typedef_return_type.endswith("*"):
                        f.write("\treturn nullptr;\n")
                    elif typedef_return_type.startswith("E"):
                        f.write(f"\treturn ({typedef_return_type})0;\n")
                    elif typedef_return_type.endswith("_t") or typedef_return_type == "AudioPlayback_Status":
                        f.write(f"\treturn {typedef_return_type}();\n")
                    else:
                        print(f"UNHANDLED RETURN TYPE! {typedef_return_type}")
            f.write("}\n\n")


def write_interfaces(f, typedef_map, interfaces):
    for interf in interfaces:
        if "accessors" in interf:
            for accessor in interf["accessors"]:
                f.write(f"S_API {interf['classname']} *{accessor['name_flat']}()")
                f.write("{ return nullptr; }\n\n")

        for method in interf["methods"]:
            if "methodname_flat" not in method:
                continue
            return_type = method["returntype_flat"] if "returntype_flat" in method else method["returntype"]
            # Write signature
            f.write(f"S_API {return_type} {method['methodname_flat']}(")

            # Write params
            # self as interface first
            f.write(f"{interf['classname']} * self")
            if len(method["params"]) > 0:
                f.write(", ")
            for i, param in enumerate(method["params"]):
                param_type = param["paramtype_flat"] if "paramtype_flat" in param else param["paramtype"]
                f.write(f"{param_type} {param['paramname']}")
                if i < len(method["params"]) - 1:
                    f.write(", ")

            f.write(") {\n")
            typedef_return_type = typedef_map.get(return_type, return_type)
            match typedef_return_type:
                case "void":
                    pass
                case "float":
                    f.write("\treturn 0.0f;\n")
                case (
                    "unsigned short"
                    | "short"
                    | "int"
                    | "long long"
                    | "unsigned long long"
                    | "unsigned int"
                    | "unsigned char"
                    | "uint64_steamid"
                ):
                    f.write("\treturn 0;\n")
                case "bool":
                    f.write("\treturn false;\n")
                case _:
                    if typedef_return_type.endswith("*"):
                        f.write("\treturn nullptr;\n")
                    elif typedef_return_type.startswith("E"):
                        f.write(f"\treturn ({typedef_return_type})0;\n")
                    elif typedef_return_type.endswith("_t") or typedef_return_type == "AudioPlayback_Status":
                        f.write(f"\treturn {typedef_return_type}();\n")
                    else:
                        print(f"UNHANDLED RETURN TYPE! {typedef_return_type}")
            f.write("}\n\n")


def generate_stub(target, source, env):
    d = json.load(open(source[0].path, "r"))

    with open(target[0].path, "w") as f:
        f.write("#ifdef USE_STEAMWORKS_STUB\n")
        f.write('#include "steam/steam_api_flat.h"\n\n')
        td_map = generate_type_map(d["typedefs"])
        write_interfaces(f, td_map, d["interfaces"])
        write_struct_methods(f, td_map, d["structs"])
        f.write("#endif // USE_STEAMWORKS_STUB\n")
