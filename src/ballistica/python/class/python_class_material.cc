// Released under the MIT License. See LICENSE for details.

#include "ballistica/python/class/python_class_material.h"

#include <string>
#include <vector>

#include "ballistica/dynamics/material/impact_sound_material_action.h"
#include "ballistica/dynamics/material/material.h"
#include "ballistica/dynamics/material/material_component.h"
#include "ballistica/dynamics/material/material_condition_node.h"
#include "ballistica/dynamics/material/node_message_material_action.h"
#include "ballistica/dynamics/material/node_mod_material_action.h"
#include "ballistica/dynamics/material/node_user_message_material_action.h"
#include "ballistica/dynamics/material/part_mod_material_action.h"
#include "ballistica/dynamics/material/python_call_material_action.h"
#include "ballistica/dynamics/material/roll_sound_material_action.h"
#include "ballistica/dynamics/material/skid_sound_material_action.h"
#include "ballistica/dynamics/material/sound_material_action.h"
#include "ballistica/game/game.h"
#include "ballistica/game/host_activity.h"
#include "ballistica/python/python.h"

namespace ballistica {

// Ignore signed bitwise stuff since python macros do a lot of it.
#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#pragma ide diagnostic ignored "RedundantCast"

bool PythonClassMaterial::s_create_empty_ = false;
PyTypeObject PythonClassMaterial::type_obj;

static void DoAddConditions(PyObject* cond_obj,
                            Object::Ref<MaterialConditionNode>* c);
static void DoAddAction(PyObject* actions_obj,
                        std::vector<Object::Ref<MaterialAction> >* actions);

// Attrs we expose through our custom getattr/setattr.
#define ATTR_LABEL "label"

// The set we expose via dir().
static const char* extra_dir_attrs[] = {ATTR_LABEL, nullptr};

void PythonClassMaterial::SetupType(PyTypeObject* obj) {
  PythonClass::SetupType(obj);
  obj->tp_name = "ba.Material";
  obj->tp_repr = (reprfunc)tp_repr;
  obj->tp_basicsize = sizeof(PythonClassMaterial);

  // clang-format off
  obj->tp_doc =
      "Material(label: str = None)\n"
      "\n"
      "An entity applied to game objects to modify collision behavior.\n"
      "\n"
      "Category: Gameplay Classes\n"
      "\n"
      "A material can affect physical characteristics, generate sounds,\n"
      "or trigger callback functions when collisions occur.\n"
      "\n"
      "Materials are applied to 'parts', which are groups of one or more\n"
      "rigid bodies created as part of a ba.Node.  Nodes can have any number\n"
      "of parts, each with its own set of materials. Generally materials are\n"
      "specified as array attributes on the Node. The 'spaz' node, for\n"
      "example, has various attributes such as 'materials',\n"
      "'roller_materials', and 'punch_materials', which correspond to the\n"
      "various parts it creates.\n"
      "\n"
      "Use ba.Material() to instantiate a blank material, and then use its\n"
      "add_actions() method to define what the material does.\n"
      "\n"
      "Attributes:\n"
      "\n"
      "    " ATTR_LABEL ": str\n"
      "        A label for the material; only used for debugging.\n";
  // clang-format on

  obj->tp_new = tp_new;
  obj->tp_dealloc = (destructor)tp_dealloc;
  obj->tp_methods = tp_methods;
  obj->tp_getattro = (getattrofunc)tp_getattro;
  obj->tp_setattro = (setattrofunc)tp_setattro;
}

auto PythonClassMaterial::tp_new(PyTypeObject* type, PyObject* args,
                                 PyObject* keywds) -> PyObject* {
  auto* self = reinterpret_cast<PythonClassMaterial*>(type->tp_alloc(type, 0));
  if (self) {
    BA_PYTHON_TRY;

    // Do anything that might throw an exception *before* our placement-new
    // stuff so we don't have to worry about cleaning it up on errors.
    if (!InGameThread()) {
      throw Exception(
          "ERROR: " + std::string(type_obj.tp_name)
          + " objects must only be created in the game thread (current is ("
          + GetCurrentThreadName() + ").");
    }
    PyObject* name_obj = Py_None;
    std::string name;
    Object::Ref<Material> m;
    if (!s_create_empty_) {
      static const char* kwlist[] = {"label", nullptr};
      if (!PyArg_ParseTupleAndKeywords(args, keywds, "|O",
                                       const_cast<char**>(kwlist), &name_obj)) {
        return nullptr;
      }
      if (name_obj != Py_None) {
        name = Python::GetPyString(name_obj);
      } else {
        name = Python::GetPythonFileLocation();
      }

      if (HostActivity* host_activity = Context::current().GetHostActivity()) {
        m = host_activity->NewMaterial(name);
        m->set_py_object(reinterpret_cast<PyObject*>(self));
      } else {
        throw Exception("Can't create materials in this context.",
                        PyExcType::kContext);
      }
    }
    self->material_ = new Object::Ref<Material>(m);
    BA_PYTHON_NEW_CATCH;
  }
  return reinterpret_cast<PyObject*>(self);
}

void PythonClassMaterial::Delete(Object::Ref<Material>* m) {
  assert(InGameThread());

  // If we're the py-object for a material, clear them out.
  if (m->exists()) {
    assert((*m)->py_object() != nullptr);
    (*m)->set_py_object(nullptr);
  }
  delete m;
}

void PythonClassMaterial::tp_dealloc(PythonClassMaterial* self) {
  BA_PYTHON_TRY;

  // These have to be deleted in the game thread - push a call if
  // need be.. otherwise do it immediately.
  if (!InGameThread()) {
    Object::Ref<Material>* ptr = self->material_;
    g_game->PushCall([ptr] { Delete(ptr); });
  } else {
    Delete(self->material_);
  }
  BA_PYTHON_DEALLOC_CATCH;
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

auto PythonClassMaterial::tp_repr(PythonClassMaterial* self) -> PyObject* {
  BA_PYTHON_TRY;
  return Py_BuildValue(
      "s",
      std::string("<ba.Material at " + Utils::PtrToString(self) + ">").c_str());
  BA_PYTHON_CATCH;
}

auto PythonClassMaterial::tp_getattro(PythonClassMaterial* self, PyObject* attr)
    -> PyObject* {
  BA_PYTHON_TRY;

  // Assuming this will always be a str?
  assert(PyUnicode_Check(attr));

  const char* s = PyUnicode_AsUTF8(attr);

  if (!strcmp(s, ATTR_LABEL)) {
    Material* material = self->material_->get();
    if (!material) {
      throw Exception("Invalid Material.", PyExcType::kNotFound);
    }
    return PyUnicode_FromString(material->label().c_str());
  }

  // Fall back to generic behavior.
  PyObject* val;
  val = PyObject_GenericGetAttr(reinterpret_cast<PyObject*>(self), attr);
  return val;
  BA_PYTHON_CATCH;
}

auto PythonClassMaterial::tp_setattro(PythonClassMaterial* self, PyObject* attr,
                                      PyObject* val) -> int {
  BA_PYTHON_TRY;
  assert(PyUnicode_Check(attr));

  throw Exception("Attr '" + std::string(PyUnicode_AsUTF8(attr))
                      + "' is not settable on Material objects.",
                  PyExcType::kAttribute);

  // return PyObject_GenericSetAttr(reinterpret_cast<PyObject*>(self), attr,
  // val);
  BA_PYTHON_INT_CATCH;
}

auto PythonClassMaterial::Dir(PythonClassMaterial* self) -> PyObject* {
  BA_PYTHON_TRY;

  // Start with the standard python dir listing.
  PyObject* dir_list = Python::generic_dir(reinterpret_cast<PyObject*>(self));
  assert(PyList_Check(dir_list));

  // ..and add in our custom attr names.
  for (const char** name = extra_dir_attrs; *name != nullptr; name++) {
    PyList_Append(
        dir_list,
        PythonRef(PyUnicode_FromString(*name), PythonRef::kSteal).get());
  }
  PyList_Sort(dir_list);
  return dir_list;

  BA_PYTHON_CATCH;
}

auto PythonClassMaterial::AddActions(PythonClassMaterial* self, PyObject* args,
                                     PyObject* keywds) -> PyObject* {
  BA_PYTHON_TRY;
  assert(InGameThread());
  PyObject* conditions_obj{Py_None};
  PyObject* actions_obj{nullptr};
  const char* kwlist[] = {"actions", "conditions", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "O|O",
                                   const_cast<char**>(kwlist), &actions_obj,
                                   &conditions_obj)) {
    return nullptr;
  }

  Object::Ref<MaterialConditionNode> conditions;
  if (conditions_obj != Py_None) {
    DoAddConditions(conditions_obj, &conditions);
  }

  Material* m = self->material_->get();
  if (!m) {
    throw Exception("Invalid Material.", PyExcType::kNotFound);
  }
  std::vector<Object::Ref<MaterialAction> > actions;
  if (PyTuple_Check(actions_obj)) {
    Py_ssize_t size = PyTuple_GET_SIZE(actions_obj);
    if (size > 0) {
      // If the first item is a string, process this tuple as a single action.
      if (PyUnicode_Check(PyTuple_GET_ITEM(actions_obj, 0))) {
        DoAddAction(actions_obj, &actions);
      } else {
        // Otherwise each item is assumed to be an action.
        for (Py_ssize_t i = 0; i < size; i++) {
          DoAddAction(PyTuple_GET_ITEM(actions_obj, i), &actions);
        }
      }
    }
  } else {
    PyErr_SetString(PyExc_AttributeError,
                    "expected a tuple for \"actions\" argument");
    return nullptr;
  }
  m->AddComponent(Object::New<MaterialComponent>(conditions, actions));

  Py_RETURN_NONE;
  BA_PYTHON_CATCH;
}

PyMethodDef PythonClassMaterial::tp_methods[] = {
    {"add_actions", (PyCFunction)AddActions, METH_VARARGS | METH_KEYWORDS,
     "add_actions(actions: Tuple, conditions: Optional[Tuple] = None)\n"
     "  -> None\n"
     "\n"
     "Add one or more actions to the material, optionally with conditions.\n"
     "\n"
     "Conditions:\n"
     "\n"
     "Conditions are provided as tuples which can be combined to form boolean\n"
     "logic. A single condition might look like ('condition_name', cond_arg),\n"
     "or a more complex nested one might look like (('some_condition',\n"
     "cond_arg), 'or', ('another_condition', cond2_arg)).\n"
     "\n"
     "'and', 'or', and 'xor' are available to chain together 2 conditions, as\n"
     "  seen above.\n"
     "\n"
     "Available Conditions:\n"
     "\n"
     "('they_have_material', material) - does the part we\'re hitting have a\n"
     "  given ba.Material?\n"
     "\n"
     "('they_dont_have_material', material) - does the part we\'re hitting\n"
     "  not have a given ba.Material?\n"
     "\n"
     "('eval_colliding') - is 'collide' true at this point in material\n"
     "  evaluation? (see the modify_part_collision action)\n"
     "\n"
     "('eval_not_colliding') - is 'collide' false at this point in material\n"
     "  evaluation? (see the modify_part_collision action)\n"
     "\n"
     "('we_are_younger_than', age) - is our part younger than 'age'\n"
     "  (in milliseconds)?\n"
     "\n"
     "('we_are_older_than', age) - is our part older than 'age'\n"
     "  (in milliseconds)?\n"
     "\n"
     "('they_are_younger_than', age) - is the part we're hitting younger than\n"
     "  'age' (in milliseconds)?\n"
     "\n"
     "('they_are_older_than', age) - is the part we're hitting older than\n"
     "  'age' (in milliseconds)?\n"
     "\n"
     "('they_are_same_node_as_us') - does the part we're hitting belong to\n"
     "  the same ba.Node as us?\n"
     "\n"
     "('they_are_different_node_than_us') - does the part we're hitting\n"
     "  belong to a different ba.Node than us?\n"
     "\n"
     "Actions:\n"
     "\n"
     "In a similar manner, actions are specified as tuples. Multiple actions\n"
     "can be specified by providing a tuple of tuples.\n"
     "\n"
     "Available Actions:\n"
     "\n"
     "('call', when, callable) - calls the provided callable; 'when' can be\n"
     "  either 'at_connect' or 'at_disconnect'. 'at_connect' means to fire\n"
     "  when the two parts first come in contact; 'at_disconnect' means to\n"
     "  fire once they cease being in contact.\n"
     "\n"
     "('message', who, when, message_obj) - sends a message object; 'who' can\n"
     "  be either 'our_node' or 'their_node', 'when' can be 'at_connect' or\n"
     "  'at_disconnect', and message_obj is the message object to send.\n"
     "  This has the same effect as calling the node's handlemessage()\n"
     "  method.\n"
     "\n"
     "('modify_part_collision', attr, value) - changes some characteristic\n"
     "  of the physical collision that will occur between our part and their\n"
     "  part.  This change will remain in effect as long as the two parts\n"
     "  remain overlapping. This means if you have a part with a material\n"
     "  that turns 'collide' off against parts younger than 100ms, and it\n"
     "  touches another part that is 50ms old, it will continue to not\n"
     "  collide with that part until they separate, even if the 100ms\n"
     "  threshold is passed. Options for attr/value are: 'physical' (boolean\n"
     "  value; whether a *physical* response will occur at all), 'friction'\n"
     "  (float value; how friction-y the physical response will be),\n"
     "  'collide' (boolean value; whether *any* collision will occur at all,\n"
     "  including non-physical stuff like callbacks), 'use_node_collide'\n"
     "  (boolean value; whether to honor modify_node_collision overrides for\n"
     "  this collision), 'stiffness' (float value, how springy the physical\n"
     "  response is), 'damping' (float value, how damped the physical\n"
     "  response is), 'bounce' (float value; how bouncy the physical response\n"
     "  is).\n"
     "\n"
     "('modify_node_collision', attr, value) - similar to\n"
     "  modify_part_collision, but operates at a node-level.\n"
     "  collision attributes set here will remain in effect as long as\n"
     "  *anything* from our part's node and their part's node overlap.\n"
     "  A key use of this functionality is to prevent new nodes from\n"
     "  colliding with each other if they appear overlapped;\n"
     "  if modify_part_collision is used, only the individual parts that\n"
     "  were overlapping would avoid contact, but other parts could still\n"
     "  contact leaving the two nodes 'tangled up'.  Using\n"
     "  modify_node_collision ensures that the nodes must completely\n"
     "  separate before they can start colliding.  Currently the only attr\n"
     "  available here is 'collide' (a boolean value).\n"
     "\n"
     "('sound', sound, volume) - plays a ba.Sound when a collision occurs, at\n"
     "  a given volume, regardless of the collision speed/etc.\n"
     "\n"
     "('impact_sound', sound, targetImpulse, volume) - plays a sound when a\n"
     "  collision occurs, based on the speed of impact. Provide a ba.Sound, a\n"
     "  target-impulse, and a volume.\n"
     "\n"
     "('skid_sound', sound, targetImpulse, volume) - plays a sound during a\n"
     "  collision when parts are 'scraping' against each other. Provide a\n"
     "  ba.Sound, a target-impulse, and a volume.\n"
     "\n"
     "('roll_sound', sound, targetImpulse, volume) - plays a sound during a\n"
     "  collision when parts are 'rolling' against each other. Provide a\n"
     "  ba.Sound, a target-impulse, and a volume.\n"
     "\n"
     "# example 1: create a material that lets us ignore\n"
     "# collisions against any nodes we touch in the first\n"
     "# 100 ms of our existence; handy for preventing us from\n"
     "# exploding outward if we spawn on top of another object:\n"
     "m = ba.Material()\n"
     "m.add_actions(conditions=(('we_are_younger_than', 100),\n"
     "                         'or',('they_are_younger_than', 100)),\n"
     "             actions=('modify_node_collision', 'collide', False))\n"
     "\n"
     "# example 2: send a DieMessage to anything we touch, but cause\n"
     "# no physical response.  This should cause any ba.Actor to drop dead:\n"
     "m = ba.Material()\n"
     "m.add_actions(actions=(('modify_part_collision', 'physical', False),\n"
     "                      ('message', 'their_node', 'at_connect',\n"
     "                       ba.DieMessage())))\n"
     "\n"
     "# example 3: play some sounds when we're contacting the ground:\n"
     "m = ba.Material()\n"
     "m.add_actions(conditions=('they_have_material',\n"
     "                          shared.footing_material),\n"
     "              actions=(('impact_sound', ba.getsound('metalHit'), 2, 5),\n"
     "                       ('skid_sound', ba.getsound('metalSkid'), 2, 5)))\n"
     "\n"},
    {"__dir__", (PyCFunction)Dir, METH_NOARGS,
     "allows inclusion of our custom attrs in standard python dir()"},

    {nullptr}};

void DoAddConditions(PyObject* cond_obj,
                     Object::Ref<MaterialConditionNode>* c) {
  assert(InGameThread());
  if (PyTuple_Check(cond_obj)) {
    Py_ssize_t size = PyTuple_GET_SIZE(cond_obj);
    if (size < 1) {
      throw Exception("Malformed arguments.", PyExcType::kValue);
    }

    PyObject* first = PyTuple_GET_ITEM(cond_obj, 0);
    assert(first);

    // If the first element is a string,
    // its a leaf node; process its elements as a single statement.
    if (PyUnicode_Check(first)) {
      (*c) = Object::New<MaterialConditionNode>();
      (*c)->opmode = MaterialConditionNode::OpMode::LEAF_NODE;
      int argc;
      const char* cond_str = PyUnicode_AsUTF8(first);
      bool first_arg_is_material = false;
      if (!strcmp(cond_str, "they_have_material")) {
        argc = 1;
        first_arg_is_material = true;
        (*c)->cond = MaterialCondition::kDstIsMaterial;
      } else if (!strcmp(cond_str, "they_dont_have_material")) {
        argc = 1;
        first_arg_is_material = true;
        (*c)->cond = MaterialCondition::kDstNotMaterial;
      } else if (!strcmp(cond_str, "eval_colliding")) {
        argc = 0;
        (*c)->cond = MaterialCondition::kEvalColliding;
      } else if (!strcmp(cond_str, "eval_not_colliding")) {
        argc = 0;
        (*c)->cond = MaterialCondition::kEvalNotColliding;
      } else if (!strcmp(cond_str, "we_are_younger_than")) {
        argc = 1;
        (*c)->cond = MaterialCondition::kSrcYoungerThan;
      } else if (!strcmp(cond_str, "we_are_older_than")) {
        argc = 1;
        (*c)->cond = MaterialCondition::kSrcOlderThan;
      } else if (!strcmp(cond_str, "they_are_younger_than")) {
        argc = 1;
        (*c)->cond = MaterialCondition::kDstYoungerThan;
      } else if (!strcmp(cond_str, "they_are_older_than")) {
        argc = 1;
        (*c)->cond = MaterialCondition::kDstOlderThan;
      } else if (!strcmp(cond_str, "they_are_same_node_as_us")) {
        argc = 0;
        (*c)->cond = MaterialCondition::kSrcDstSameNode;
      } else if (!strcmp(cond_str, "they_are_different_node_than_us")) {
        argc = 0;
        (*c)->cond = MaterialCondition::kSrcDstDiffNode;
      } else {
        throw Exception(
            std::string("Invalid material condition: \"") + cond_str + "\".",
            PyExcType::kValue);
      }
      if (size != (argc + 1)) {
        throw Exception(
            std::string("Wrong number of arguments for condition: \"")
                + cond_str + "\".",
            PyExcType::kValue);
      }
      if (argc > 0) {
        if (first_arg_is_material) {
          (*c)->val1_material =
              Python::GetPyMaterial(PyTuple_GET_ITEM(cond_obj, 1));
        } else {
          PyObject* o = PyTuple_GET_ITEM(cond_obj, 1);
          if (!PyLong_Check(o)) {
            throw Exception(
                std::string("Expected int for first arg of condition: \"")
                    + cond_str + "\".",
                PyExcType::kType);
          }
          (*c)->val1 = static_cast<uint32_t>(PyLong_AsLong(o));
        }
      }
      if (argc > 1) {
        PyObject* o = PyTuple_GET_ITEM(cond_obj, 2);
        if (!PyLong_Check(o)) {
          throw Exception(
              std::string("Expected int for second arg of condition: \"")
                  + cond_str + "\".",
              PyExcType::kType);
        }
        (*c)->val1 = static_cast<uint32_t>(PyLong_AsLong(o));
      }
    } else if (PyTuple_Check(first)) {
      // First item is a tuple - assume its a tuple of size 3+2*n
      // containing tuples for odd index vals and operators for even.
      if (size < 3 || (size % 2 != 1)) {
        throw Exception("Malformed conditional statement.", PyExcType::kValue);
      }
      Object::Ref<MaterialConditionNode> c2;
      Object::Ref<MaterialConditionNode> c2_prev;
      for (Py_ssize_t i = 0; i < (size - 1); i += 2) {
        c2 = Object::New<MaterialConditionNode>();
        if (c2_prev.exists()) {
          c2->left_child = c2_prev;
        } else {
          DoAddConditions(PyTuple_GET_ITEM(cond_obj, i), &c2->left_child);
        }
        DoAddConditions(PyTuple_GET_ITEM(cond_obj, i + 2), &c2->right_child);

        // Pull a string from between to set up our opmode with.
        std::string opmode_str =
            Python::GetPyString(PyTuple_GET_ITEM(cond_obj, i + 1));
        const char* opmode = opmode_str.c_str();
        if (!strcmp(opmode, "&&") || !strcmp(opmode, "and")) {
          c2->opmode = MaterialConditionNode::OpMode::AND_OPERATOR;
        } else if (!strcmp(opmode, "||") || !strcmp(opmode, "or")) {
          c2->opmode = MaterialConditionNode::OpMode::OR_OPERATOR;
        } else if (!strcmp(opmode, "^") || !strcmp(opmode, "xor")) {
          c2->opmode = MaterialConditionNode::OpMode::XOR_OPERATOR;
        } else {
          throw Exception(
              std::string("Invalid conditional operator: \"") + opmode + "\".",
              PyExcType::kValue);
        }
        c2_prev = c2;
      }
      // Keep our lowest level.
      (*c) = c2;
    }
  } else {
    throw Exception("Conditions argument not a tuple.", PyExcType::kType);
  }
}

void DoAddAction(PyObject* actions_obj,
                 std::vector<Object::Ref<MaterialAction> >* actions) {
  assert(InGameThread());
  if (!PyTuple_Check(actions_obj)) {
    throw Exception("Expected a tuple.", PyExcType::kType);
  }
  Py_ssize_t size = PyTuple_GET_SIZE(actions_obj);
  assert(size > 0);
  PyObject* obj = PyTuple_GET_ITEM(actions_obj, 0);
  std::string type = Python::GetPyString(obj);
  if (type == "call") {
    if (size != 3) {
      throw Exception("Expected 3 values for command action tuple.",
                      PyExcType::kValue);
    }
    std::string when = Python::GetPyString(PyTuple_GET_ITEM(actions_obj, 1));
    bool at_disconnect;
    if (when == "at_connect") {
      at_disconnect = false;
    } else if (when == "at_disconnect") {
      at_disconnect = true;
    } else {
      throw Exception("Invalid command execution time: '" + when + "'.",
                      PyExcType::kValue);
    }
    PyObject* call_obj = PyTuple_GET_ITEM(actions_obj, 2);
    (*actions).push_back(Object::New<MaterialAction, PythonCallMaterialAction>(
        at_disconnect, call_obj));
  } else if (type == "message") {
    if (size < 4) {
      throw Exception("Expected >= 4 values for message action tuple.",
                      PyExcType::kValue);
    }
    std::string target = Python::GetPyString(PyTuple_GET_ITEM(actions_obj, 1));
    bool target_other_val;
    if (target == "our_node") {
      target_other_val = false;
    } else if (target == "their_node") {
      target_other_val = true;
    } else {
      throw Exception("Invalid message target: '" + target + "'.",
                      PyExcType::kValue);
    }
    std::string when = Python::GetPyString(PyTuple_GET_ITEM(actions_obj, 2));
    bool at_disconnect;
    if (when == "at_connect") {
      at_disconnect = false;
    } else if (when == "at_disconnect") {
      at_disconnect = true;
    } else {
      throw Exception("Invalid command execution time: '" + when + "'.",
                      PyExcType::kValue);
    }

    // Pull the rest of the message.
    Buffer<char> b;
    PyObject* user_message_obj = nullptr;
    Python::DoBuildNodeMessage(actions_obj, 3, &b, &user_message_obj);
    if (user_message_obj) {
      (*actions).push_back(
          Object::New<MaterialAction, NodeUserMessageMaterialAction>(
              target_other_val, at_disconnect, user_message_obj));
    } else if (b.size() > 0) {
      (*actions).push_back(
          Object::New<MaterialAction, NodeMessageMaterialAction>(
              target_other_val, at_disconnect, b.data(), b.size()));
    }
  } else if (type == "modify_node_collision") {
    if (size != 3) {
      throw Exception(
          "Expected 3 values for modify_node_collision action tuple.",
          PyExcType::kValue);
    }
    std::string attr = Python::GetPyString(PyTuple_GET_ITEM(actions_obj, 1));
    NodeCollideAttr attr_type;
    if (attr == "collide") {
      attr_type = NodeCollideAttr::kCollideNode;
    } else {
      throw Exception("Invalid node mod attr: '" + attr + "'.",
                      PyExcType::kValue);
    }

    // Pull value.
    float val = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 2));
    (*actions).push_back(
        Object::New<MaterialAction, NodeModMaterialAction>(attr_type, val));
  } else if (type == "modify_part_collision") {
    if (size != 3) {
      throw Exception(
          "Expected 3 values for modify_part_collision action tuple.",
          PyExcType::kValue);
    }
    PartCollideAttr attr_type;
    std::string attr = Python::GetPyString(PyTuple_GET_ITEM(actions_obj, 1));
    if (attr == "physical") {
      attr_type = PartCollideAttr::kPhysical;
    } else if (attr == "friction") {
      attr_type = PartCollideAttr::kFriction;
    } else if (attr == "collide") {
      attr_type = PartCollideAttr::kCollide;
    } else if (attr == "use_node_collide") {
      attr_type = PartCollideAttr::kUseNodeCollide;
    } else if (attr == "stiffness") {
      attr_type = PartCollideAttr::kStiffness;
    } else if (attr == "damping") {
      attr_type = PartCollideAttr::kDamping;
    } else if (attr == "bounce") {
      attr_type = PartCollideAttr::kBounce;
    } else {
      throw Exception("Invalid part mod attr: '" + attr + "'.",
                      PyExcType::kValue);
    }
    float val = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 2));
    (*actions).push_back(
        Object::New<MaterialAction, PartModMaterialAction>(attr_type, val));
  } else if (type == "sound") {
    if (size != 3) {
      throw Exception("Expected 3 values for sound action tuple.",
                      PyExcType::kValue);
    }
    Sound* sound = Python::GetPySound(PyTuple_GET_ITEM(actions_obj, 1));
    float volume = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 2));
    (*actions).push_back(
        Object::New<MaterialAction, SoundMaterialAction>(sound, volume));
  } else if (type == "impact_sound") {
    if (size != 4) {
      throw Exception("Expected 4 values for impact_sound action tuple.",
                      PyExcType::kValue);
    }
    PyObject* sounds_obj = PyTuple_GET_ITEM(actions_obj, 1);
    std::vector<Sound*> sounds;
    if (PySequence_Check(sounds_obj)) {
      sounds = Python::GetPySounds(sounds_obj);  // Sequence of sounds.
    } else {
      sounds.push_back(Python::GetPySound(sounds_obj));  // Single sound.
    }
    if (sounds.empty()) {
      throw Exception("Require at least 1 sound.", PyExcType::kValue);
    }
    if (Utils::HasNullMembers(sounds)) {
      throw Exception("One or more invalid sound refs passed.",
                      PyExcType::kValue);
    }
    float target_impulse = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 2));
    float volume = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 3));
    (*actions).push_back(Object::New<MaterialAction, ImpactSoundMaterialAction>(
        sounds, target_impulse, volume));
  } else if (type == "skid_sound") {
    if (size != 4) {
      throw Exception("Expected 4 values for skid_sound action tuple.",
                      PyExcType::kValue);
    }
    Sound* sound = Python::GetPySound(PyTuple_GET_ITEM(actions_obj, 1));
    float target_impulse = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 2));
    float volume = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 3));
    (*actions).push_back(Object::New<MaterialAction, SkidSoundMaterialAction>(
        sound, target_impulse, volume));
  } else if (type == "roll_sound") {
    if (size != 4) {
      throw Exception("Expected 4 values for roll_sound action tuple.",
                      PyExcType::kValue);
    }
    Sound* sound = Python::GetPySound(PyTuple_GET_ITEM(actions_obj, 1));
    float target_impulse = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 2));
    float volume = Python::GetPyFloat(PyTuple_GET_ITEM(actions_obj, 3));
    (*actions).push_back(Object::New<MaterialAction, RollSoundMaterialAction>(
        sound, target_impulse, volume));
  } else {
    throw Exception("Invalid action type: '" + type + "'.", PyExcType::kValue);
  }
}

#pragma clang diagnostic pop

}  // namespace ballistica
