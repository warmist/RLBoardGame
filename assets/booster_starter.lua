function next_cell( pos,dir )
	local dir2=dir/math.max(math.abs(dir[1]),math.abs(dir[2]))
	return pos+dir2
end
deck={}
deck.strike={
	--main
	name="Strike",
	cost=2,
	description="Perform an attack with a very big sword",
	--misc
	attack=3,
	range=1.5,
	--callbacks
	use=function ( card,game )
		local target=game.target_enemy(game.player:pos(),card.range)
		target:damage(card.attack)
		card:discard()
	end
}
deck.push={

	name="Push",
	cost=1,
	description="Force a move. Deal damage if can't move",

	attack=1,
	range=1.5,
	distance=3,

	use=function ( card,game )
		--first ask which one of enemies you want to push
		local target=game.target_enemy(game.player:pos(),card.range)
		--by default if the first "target" function is canceled. the whole
		--		card is not played and returned to the hand

		--figure out the direction of push
		local dir=target:pos()-game.player:pos()
		dir=dir:normalized()
		local start=target:pos()

		--raycast (i.e. like going straight in that direction)
		local target_cell=start+dir*card.distance
		local path,path_len=game.raycast(start,target_cell,false)
		--move the unit
		game.move(target,path)

		--then if the unit could not move ALL the way, hurt it
		print("moved:",path_len)
		if path_len<card.distance then
			target:damage(card.attack)
		end
		card:discard()
	end
}
deck.move={
	name="Run",
	cost=1,
	description="Run to the target",

	generated=true,
	range=5,
	move=true,

	use=function (card,game)
		local path=game.target_path(game.player:pos(),card.range)
		game.move(game.player,path)
		card:destroy()
	end,
	turn_end=function( card,game )
		print("Run burns up")
		card:destroy()
	end
}
--alternative way of writing the same
function wound_use(self,game)
	--get cards from "hand"
	local cards=game.get_cards(2)
	--filter cards: you can only choose to destroy non-wounds
	local tmp_cards={}
	for i,v in ipairs(cards) do
		if not v.wound then
			table.insert(tmp_cards,v)
		end
	end
	--ask which one
	local card_choice=game.target_card(tmp_cards)
	--if picked a card (e.g. you can be left with only wound in hand)
	if #card_choice then
		--destroy it
		card_choice[1]:destroy()
		--destroy self
		self:destroy()
	end
end
local wound={
	name="Wound",
	--TODO: loose game if 3 in hand should be a hint.
	description="Loose game if 3 are in hand at the end of turn",
	wound=true,
	use=wound_use,
}
local hunger={
	name="Hunger",

	description="Your hunger consumes you. Loose game if any 3 wounds are in hand at the end of the turn",
	wound=true,
	use=wound_use,
}

deck.wound=wound
deck.hunger=hunger

--[===[ ENEMIES ]===]
local function simple_enemy_turn( self,game )
	if not self.angry then
		local r,rl=game.raycast(self:pos(),game.player:pos(),true)
		if r[#r]==game.player:pos() then
			self.angry=self.angry_timeout
			print(self,"became angry")
		end
	end
	if self.angry then
		local p=game.pathfind(self:pos(),game.player:pos())
		if #p==0 then
			print(self,"where's the player? angering down")
			self.angry=self.angry-1
			return
		else
			--move up to move_dist and then if you reach player, attack
			local full_path=#p
			if full_path<=self.move_dist then
				p[#p]=nil
			else
				local step_count=math.min(#p,self.move_dist)
				for i=step_count+1,#p do
					print(p[i])
					p[i]=nil
				end
			end
			game.move(self,p)
			-- reached player
			if full_path<=self.move_dist then
				game.player:damage(self.damage_card or wound)
			end
		end
	end
end
mobs={}
mobs.goblin={
	--universal entries
	name="Goblin",
	img={'g',0.2, 0.8, 0.1},
	hp=3,
	angry_timeout=10,
	--used by simple turn logic
	move_dist=3,
	turn=simple_enemy_turn,
}
mobs.gablin={

	name="Gablin",
	img={'G',0.1, 0.9, 0.0},
	hp=6,
	angry_timeout=20,

	move_dist=2,
	turn=simple_enemy_turn,
}
return {cards=deck,mobs=mobs}