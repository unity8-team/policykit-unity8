/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Ted Gould <ted.gould@canonical.com>
 */

#include <polkitagent/polkitagent.h>

#define POLKIT_MOCK_SUBJECT_TYPE (polkit_mock_subject_get_type())
#define POLKIT_MOCK_SUBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), POLKIT_MOCK_SUBJECT_TYPE, PolkitMockSubject))
#define POLKIT_MOCK_SUBJECT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), POLKIT_MOCK_SUBJECT_TYPE, PolkitMockSubjectClass))
#define IS_POLKIT_MOCK_SUBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), POLKIT_MOCK_SUBJECT_TYPE))
#define IS_POLKIT_MOCK_SUBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), POLKIT_MOCK_SUBJECT_TYPE))
#define POLKIT_MOCK_SUBJECT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), POLKIT_MOCK_SUBJECT_TYPE, PolkitMockSubjectClass))

typedef struct _PolkitMockSubject PolkitMockSubject;
typedef struct _PolkitMockSubjectClass PolkitMockSubjectClass;

struct _PolkitMockSubjectClass
{
    GObjectClass parent_class;
};

struct _PolkitMockSubject
{
    GObject parent;
};

GType polkit_mock_subject_get_type(void);

typedef struct _PolkitMockSubjectPrivate PolkitMockSubjectPrivate;

struct _PolkitMockSubjectPrivate
{
    int dummy;
};

#define POLKIT_MOCK_SUBJECT_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), POLKIT_MOCK_SUBJECT_TYPE, PolkitMockSubjectPrivate))

static void polkit_mock_subject_class_init(PolkitMockSubjectClass* klass);
static void polkit_mock_subject_init(PolkitMockSubject* self);
static void polkit_mock_subject_dispose(GObject* object);
static void polkit_mock_subject_finalize(GObject* object);

G_DEFINE_TYPE_WITH_CODE(PolkitMockSubject,
                        polkit_mock_subject,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(POLKIT_TYPE_SUBJECT, nullptr));

static void polkit_mock_subject_class_init(PolkitMockSubjectClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(PolkitMockSubjectPrivate));

    object_class->dispose = polkit_mock_subject_dispose;
    object_class->finalize = polkit_mock_subject_finalize;
}

static void polkit_mock_subject_init(PolkitMockSubject* self)
{
}

static void polkit_mock_subject_dispose(GObject* object)
{
    G_OBJECT_CLASS(polkit_mock_subject_parent_class)->dispose(object);
}

static void polkit_mock_subject_finalize(GObject* object)
{
    G_OBJECT_CLASS(polkit_mock_subject_parent_class)->finalize(object);
}

PolkitSubject* polkit_unix_session_new_for_process_sync(gint pid, GCancellable* cancel, GError** error)
{
    return (PolkitSubject*)g_object_new(POLKIT_MOCK_SUBJECT_TYPE, nullptr);
}
