/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file NewAccount.h
 * @author Marek Kotewicz <marek@ethdev.com>
 * @date 2015
 */

#include "NewAccount.h"
#include <QMenu>
#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <libdevcore/Log.h>
#include <libethcore/KeyManager.h>
#include <libethereum/Client.h>
#include "ui_NewAccount.h"
using namespace std;
using namespace dev;
using namespace az;
using namespace eth;

bool beginsWith(Address _a, bytes const& _b)
{
	for (unsigned i = 0; i < min<unsigned>(20, _b.size()); ++i)
		if (_a[i] != _b[i])
			return false;
	return true;
}

DEV_AZ_NOTE_PLUGIN(NewAccount);

NewAccount::NewAccount(MainFace* _m):
	Plugin(_m, "NewAccount")
{
	connect(addMenuItem("New Account...", "menuTools", true), SIGNAL(triggered()), SLOT(create()));
}

NewAccount::~NewAccount()
{
}

void NewAccount::create()
{
	QDialog d;
	Ui::NewAccount u;
	u.setupUi(&d);
	u.keysPath->setText(QString::fromStdString(getDataDir("web3/keys")));

	auto updateStatus = [&]()
	{
		bool useMaster = u.useMaster->isChecked();
		u.password->setEnabled(!useMaster);
		u.confirm->setEnabled(!useMaster);
		u.hint->setEnabled(!useMaster);
		u.status->setText("");
		bool ok = (useMaster || (!u.password->text().isEmpty() && u.password->text() == u.confirm->text()));
		if (!ok)
			u.status->setText("Passphrases must match");
		if (u.keyName->text().isEmpty())
		{
			ok = false;
			u.status->setText("Name must not be empty");
		}
		u.create->setEnabled(ok);
	};

	connect(u.useMaster, &QCheckBox::toggled, [&]() { updateStatus(); });
	connect(u.useOwn, &QCheckBox::toggled, [&]() { updateStatus(); });
	connect(u.password, &QLineEdit::textChanged, [&]() { updateStatus(); });
	connect(u.keyName, &QLineEdit::textChanged, [&]() { updateStatus(); });
	connect(u.confirm, &QLineEdit::textChanged, [&]() { updateStatus(); });

	updateStatus();
	if (d.exec() == QDialog::Accepted)
	{
		Type v = (Type)u.keyType->currentIndex();
		KeyPair p = newKeyPair(v);
		QString s = u.keyName->text();
		if (u.useMaster->isChecked())
			main()->keyManager().import(p.secret(), s.toStdString());
		else
		{
			std::string hint = u.hint->toPlainText().toStdString();
			std::string password = u.password->text().toStdString();
			main()->keyManager().import(p.secret(), s.toStdString(), password, hint);
		}

		main()->noteKeysChanged();
	}
}

KeyPair NewAccount::newKeyPair(Type _type)
{
	KeyPair p;
	bool keepGoing = true;
	unsigned done = 0;
	function<void()> f = [&]() {
		KeyPair lp;
		while (keepGoing)
		{
			done++;
			if (done % 1000 == 0)
				cnote << "Tried" << done << "keys";
			lp = KeyPair::create();
			auto a = lp.address();
			if (_type == NoVanity ||
				(_type == DirectICAP && !a[0]) ||
				(_type == FirstTwo && a[0] == a[1]) ||
				(_type == FirstTwoNextTwo && a[0] == a[1] && a[2] == a[3]) ||
				(_type == FirstThree && a[0] == a[1] && a[1] == a[2]) ||
				(_type == FirstFour && a[0] == a[1] && a[1] == a[2] && a[2] == a[3])
			)
				break;
		}
		if (keepGoing)
			p = lp;
		keepGoing = false;
	};

	vector<std::thread*> ts;
	for (unsigned t = 0; t < std::thread::hardware_concurrency() - 1; ++t)
		ts.push_back(new std::thread(f));
	f();

	for (std::thread* t: ts)
	{
		t->join();
		delete t;
	}
	return p;
}
